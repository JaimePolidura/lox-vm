#include "lllil.h"

static void adjust_node_inputs_to_req_operand_type_ll_lox_ir(struct lllil_control*, struct lox_ir_control_node*);
static struct lox_ir_ll_operand alloc_new_v_register(struct lllil_control * lllil, bool is_float);

struct lllil * alloc_low_level_lox_ir_lowerer(struct lox_ir * lox_ir) {
    struct lllil * lllil = NATIVE_LOX_MALLOC(sizeof(struct lllil));
    struct arena arena;
    lllil->lox_ir = lox_ir;
    init_arena(&arena);
    lllil->lllil_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&lllil->stack_slots_by_block, &lllil->lllil_allocator.lox_allocator);
    init_u64_set(&lllil->processed_blocks, &lllil->lllil_allocator.lox_allocator);
    lllil->last_phi_resolution_v_reg_allocated = lox_ir->function->n_locals;

    return lllil;
}

void free_low_level_lox_ir_lowerer(struct lllil * lllil) {
    free_arena(&lllil->lllil_allocator.arena);
    NATIVE_LOX_FREE(lllil);
}

uint16_t allocate_stack_slot_lllil(
        struct lllil * lllil,
        struct lox_ir_block * block,
        size_t required_size
) {
    if(!contains_u64_hash_table(&lllil->stack_slots_by_block, (uint64_t) block)){
        struct ptr_arraylist * stack_slots = alloc_ptr_arraylist(&lllil->lllil_allocator.lox_allocator);
        put_u64_hash_table(&lllil->stack_slots_by_block, (uint64_t) block, stack_slots);
    }

    struct ptr_arraylist * stack_slots = get_u64_hash_table(&lllil->stack_slots_by_block, (uint64_t) block);
    append_ptr_arraylist(stack_slots, (void *) required_size);

    return stack_slots->in_use - 1;
}

void add_lowered_node_lllil_control(
        struct lllil_control * lllil_control,
        struct lox_ir_control_node * lowered_node_to_add
) {
    struct lox_ir_control_node * prev_node = lllil_control->last_node_lowered;
    struct lox_ir_block * block = lllil_control->control_node_to_lower->block;

    add_after_control_node_block_lox_ir(lllil_control->lllil->lox_ir, block, prev_node, lowered_node_to_add);
    lllil_control->last_node_lowered = lowered_node_to_add;

    adjust_node_inputs_to_req_operand_type_ll_lox_ir(lllil_control, lowered_node_to_add);
}

typedef enum {
    //REGISTER, IMMEDIATE, MEMORY_ADDRESS, STACK_SLOT
    REQUIRED_INPUT_OPERAND_TYPE_ALL,
    //REGISTER, IMMEDIATE
    REQUIRED_INPUT_OPERAND_TYPE_VALUE,
    //REGISTER, MEMORY_ADDRESS
    REQUIRED_INPUT_OPERAND_TYPE_MEMORY,
} required_input_operand_type_t;

static void make_operand_type_adjustment(struct lllil_control*,struct lox_ir_control_node*,
                                         struct lox_ir_ll_operand*,required_input_operand_type_t);
static void make_operand_type_adjustment_all(struct lllil_control*,struct lox_ir_control_node*,struct lox_ir_ll_operand*);
static void make_operand_type_adjustment_value(struct lllil_control*,struct lox_ir_control_node*,struct lox_ir_ll_operand*);
static void make_operand_type_adjustment_memory(struct lllil_control*,struct lox_ir_control_node*,struct lox_ir_ll_operand*);
static void adjust_flags_to_register_operand(struct lllil_control*, struct lox_ir_ll_operand*, struct lox_ir_control_node*);
static void adjust_memory_address_to_register_operand(struct lllil_control*, struct lox_ir_ll_operand*, struct lox_ir_control_node*);
static void adjust_immediate_to_register_operand(struct lllil_control*, struct lox_ir_ll_operand*, struct lox_ir_control_node*);

void adjust_node_inputs_to_req_operand_type_ll_lox_ir(struct lllil_control * lllil, struct lox_ir_control_node * control) {
    switch (control->type) {
        case LOX_IR_CONTROL_NODE_LL_COND_FUNCTION_CALL: {
            struct lox_ir_control_ll_cond_function_call * cond_call = (struct lox_ir_control_ll_cond_function_call *) control;
            adjust_node_inputs_to_req_operand_type_ll_lox_ir(lllil, &cond_call->call->control);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL: {
            struct lox_ir_control_ll_function_call * call = (struct lox_ir_control_ll_function_call *) control;
            for (int i = 0; i < call->arguments.in_use; i++) {
                struct lox_ir_ll_operand * argument = call->arguments.values[i];
                make_operand_type_adjustment(lllil, control, argument, REQUIRED_INPUT_OPERAND_TYPE_ALL);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_MOVE: {
            struct lox_ir_control_ll_move * move = (struct lox_ir_control_ll_move *) control;
            make_operand_type_adjustment(lllil, control, &move->from, REQUIRED_INPUT_OPERAND_TYPE_ALL);
            make_operand_type_adjustment(lllil, control, &move->to, REQUIRED_INPUT_OPERAND_TYPE_MEMORY);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_BINARY: {
            struct lox_ir_control_ll_binary * binary = (struct lox_ir_control_ll_binary *) control;
            make_operand_type_adjustment(lllil, control, &binary->left, REQUIRED_INPUT_OPERAND_TYPE_VALUE);
            make_operand_type_adjustment(lllil, control, &binary->right, REQUIRED_INPUT_OPERAND_TYPE_VALUE);
            make_operand_type_adjustment(lllil, control, &binary->result, REQUIRED_INPUT_OPERAND_TYPE_VALUE);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_COMPARATION: {
            struct lox_ir_control_ll_comparation * comp = (struct lox_ir_control_ll_comparation *) control;
            make_operand_type_adjustment(lllil, control, &comp->left, REQUIRED_INPUT_OPERAND_TYPE_VALUE);
            make_operand_type_adjustment(lllil, control, &comp->left, REQUIRED_INPUT_OPERAND_TYPE_VALUE);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_UNARY: {
            struct lox_ir_control_ll_unary * unary = (struct lox_ir_control_ll_unary *) control;
            make_operand_type_adjustment(lllil, control, &unary->operand, REQUIRED_INPUT_OPERAND_TYPE_VALUE);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_RETURN: {
            struct lox_ir_control_ll_return * ret = (struct lox_ir_control_ll_return *) control;
            make_operand_type_adjustment(lllil, control, &ret->to_return, REQUIRED_INPUT_OPERAND_TYPE_ALL);
            break;
        }
        default:
            lox_assert_failed("lllil.c::adjust_node_inputs_to_req_operand_type_ll_lox_ir", "Unknown control lox type %i", control->type);
    }
}

static void make_operand_type_adjustment(
        struct lllil_control * lllil,
        struct lox_ir_control_node * control_node,
        struct lox_ir_ll_operand * operand,
        required_input_operand_type_t type
) {
    switch (type) {
        case REQUIRED_INPUT_OPERAND_TYPE_ALL: make_operand_type_adjustment_all(lllil, control_node, operand); break;
        case REQUIRED_INPUT_OPERAND_TYPE_VALUE: make_operand_type_adjustment_value(lllil, control_node, operand); break;
        case REQUIRED_INPUT_OPERAND_TYPE_MEMORY: make_operand_type_adjustment_memory(lllil, control_node, operand); break;
        default:
            lox_assert_failed("lllil.c::make_operand_type_adjustment", "Unknown required input operand type %i", operand->type);
    }
}

static void make_operand_type_adjustment_all(
        struct lllil_control * lllil,
        struct lox_ir_control_node * control,
        struct lox_ir_ll_operand * operand
) {
    switch (operand->type) {
        case LOX_IR_LL_OPERAND_FLAGS: {
            adjust_flags_to_register_operand(lllil, operand, control);
            break;
        }
        case LOX_IR_LL_OPERAND_REGISTER:
        case LOX_IR_LL_OPERAND_IMMEDIATE:
        case LOX_IR_LL_OPERAND_MEMORY_ADDRESS:
        case LOX_IR_LL_OPERAND_STACK_SLOT:
        case LOX_IR_LL_OPERAND_PHI_V_REGISTER:
            break;
        default:
            lox_assert_failed("lllil.c::make_operand_type_adjustment_all", "Unknown operand type %i", operand->type);
    }
}

static void make_operand_type_adjustment_value(
        struct lllil_control * lllil,
        struct lox_ir_control_node * control,
        struct lox_ir_ll_operand * operand
) {
    switch (operand->type) {
        case LOX_IR_LL_OPERAND_MEMORY_ADDRESS: adjust_memory_address_to_register_operand(lllil, operand, control); break;
        case LOX_IR_LL_OPERAND_FLAGS: adjust_flags_to_register_operand(lllil, operand, control); break;
        case LOX_IR_LL_OPERAND_REGISTER:
        case LOX_IR_LL_OPERAND_IMMEDIATE:
            break;
        default:
            lox_assert_failed("lllil.c::make_operand_type_adjustment_value", "Unknown operand type %i", operand->type);
    }
}

static void make_operand_type_adjustment_memory(
        struct lllil_control * lllil,
        struct lox_ir_control_node * control,
        struct lox_ir_ll_operand * operand
) {
    switch (operand->type) {
        case LOX_IR_LL_OPERAND_REGISTER:
        case LOX_IR_LL_OPERAND_MEMORY_ADDRESS:
        case LOX_IR_LL_OPERAND_STACK_SLOT:
            break;
        case LOX_IR_LL_OPERAND_FLAGS: adjust_flags_to_register_operand(lllil, operand, control); break;
        case LOX_IR_LL_OPERAND_IMMEDIATE: adjust_immediate_to_register_operand(lllil, operand, control); break;
            break;
        default:
            lox_assert_failed("lllil.c::make_operand_type_adjustment_memory", "Unknown operand type %i", operand->type);
    }
}

static void adjust_flags_to_register_operand(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand * operand,
        struct lox_ir_control_node * control
) {
    struct lox_ir_control_ll_unary * unary = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_UNARY,
            struct lox_ir_control_ll_unary, NULL, LOX_IR_ALLOCATOR(lllil->lllil->lox_ir));
    unary->operator = operand->flags_operator != OP_EQUAL ? (operand->flags_operator == OP_GREATER
            ? UNARY_LL_LOX_IR_FLAGS_GREATER_TO_NATIVE_BOOL : UNARY_LL_LOX_IR_FLAGS_LESS_TO_NATIVE_BOOL) : UNARY_LL_LOX_IR_FLAGS_EQ_TO_NATIVE_BOOL;
    unary->operand = alloc_new_v_register(lllil, false);
    add_before_control_node_block_lox_ir(lllil->lllil->lox_ir, control->block, control, &unary->control);

    *operand = unary->operand;
}

static void adjust_memory_address_to_register_operand(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand * operand,
        struct lox_ir_control_node * control
) {
    struct lox_ir_control_ll_move * move = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_MOVE,
            struct lox_ir_control_ll_move, NULL, LOX_IR_ALLOCATOR(lllil->lllil->lox_ir));
    move->from = *operand;
    move->to = alloc_new_v_register(lllil, false);
    add_before_control_node_block_lox_ir(lllil->lllil->lox_ir, control->block, control, &move->control);

    //Update with new operand
    *operand = move->to;
}

static void adjust_immediate_to_register_operand(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand * operand,
        struct lox_ir_control_node * control
) {
    struct lox_ir_control_ll_move * move = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_MOVE,
            struct lox_ir_control_ll_move, NULL, LOX_IR_ALLOCATOR(lllil->lllil->lox_ir));
    move->from = *operand;
    move->to = alloc_new_v_register(lllil, false);
    add_before_control_node_block_lox_ir(lllil->lllil->lox_ir, control->block, control, &move->control);

    //Update with new operand
    *operand = move->to;
}

static struct lox_ir_ll_operand alloc_new_v_register(struct lllil_control * lllil, bool is_float) {
    struct ssa_name ssa_name = alloc_ssa_name_lox_ir(lllil->lllil->lox_ir, 1, "temp",
            lllil->control_node_to_lower->block, NULL);
    return V_REG_TO_OPERAND(CREATE_V_REG(ssa_name, is_float));
}