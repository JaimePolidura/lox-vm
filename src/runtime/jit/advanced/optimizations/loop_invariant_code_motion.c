#include "loop_invariant_code_motion.h"

extern void runtime_panic(char * format, ...);

struct licm {
    struct ssa_ir * ssa_ir;
    struct arena_lox_allocator allocator;
    struct stack_list pending; //Stack of struct ssa_control_node to check
};

struct licm * alloc_loop_invariant_code_motion(struct ssa_ir *);
void free_loop_invariant_code_motion(struct licm *);
static void initialization(struct licm *);
static bool initialization_block(struct ssa_block *, void *);
static void move_loop_invariants(struct licm *);
static bool can_control_node_be_checked_for_loop_invariant(struct ssa_control_node *node);
static bool try_move_up_loop_invariant(struct licm *licm, struct ssa_control_node *node);
static bool is_data_node_loop_invariant(struct licm *, struct ssa_control_node *, struct ssa_data_node *);
static bool try_move_up_loop_invariant_data_node(struct ssa_data_node*, void**,struct ssa_data_node*, void*);
static void move_up_loop_invariant(struct licm *, void **, struct ssa_control_node *, struct ssa_data_node *);
static struct ssa_block * get_block_to_move_invariant(struct licm *, struct ssa_block * loop_condition_block);

void perform_loop_invariant_code_motion(struct ssa_ir * ssa_ir) {
    struct licm * licm = alloc_loop_invariant_code_motion(ssa_ir);

    initialization(licm);
    move_loop_invariants(licm);

    free_loop_invariant_code_motion(licm);
}

static void move_loop_invariants(struct licm * licm) {
    while (!is_empty_stack_list(&licm->pending)) {
        struct ssa_control_node * pending_control_node = pop_stack_list(&licm->pending);

        if (can_control_node_be_checked_for_loop_invariant(pending_control_node)) {
            bool some_invariant_moved = try_move_up_loop_invariant(licm, pending_control_node);
            if(some_invariant_moved && BELONGS_TO_LOOP_BODY_BLOCK(pending_control_node->block)){
                push_stack_list(&licm->pending, pending_control_node);
            }
        }
    }
}

static void initialization(struct licm * licm) {
    for_each_ssa_block(
            licm->ssa_ir->first_block,
            &licm->allocator.lox_allocator,
            licm,
            initialization_block
    );
}

static bool initialization_block(struct ssa_block * current_block, void * extra) {
    if(BELONGS_TO_LOOP_BODY_BLOCK(current_block)) {
        struct licm * licm = extra;

        for (struct ssa_control_node * current = current_block->first;;current = current->next) {
            push_stack_list(&licm->pending, current);

            if(current == current_block->last) {
                break;
            }
        }
    }

    return true;
}

struct try_move_up_loop_invariant_data_node_struct {
    struct ssa_control_node * control_node;
    struct licm * licm;
    bool some_loop_invariant_moved;
};

static bool try_move_up_loop_invariant(struct licm * licm, struct ssa_control_node * node) {
    struct try_move_up_loop_invariant_data_node_struct consumer_struct = (struct try_move_up_loop_invariant_data_node_struct) {
        .some_loop_invariant_moved = false,
        .control_node = node,
        .licm = licm,
    };

    for_each_data_node_in_control_node(
            node,
            &consumer_struct,
            SSA_DATA_NODE_OPT_PRE_ORDER | SSA_DATA_NODE_OPT_RECURSIVE | SSA_DATA_NODE_OPT_NOT_TERMINATORS,
            &try_move_up_loop_invariant_data_node
    );

    return consumer_struct.some_loop_invariant_moved;
}

static bool try_move_up_loop_invariant_data_node(
        struct ssa_data_node * _,
        void ** parent_ptr,
        struct ssa_data_node * current_data_node,
        void * extra
) {
    struct try_move_up_loop_invariant_data_node_struct * consumer_struct = extra;

    if(is_data_node_loop_invariant(consumer_struct->licm, consumer_struct->control_node, current_data_node)) {
        move_up_loop_invariant(consumer_struct->licm, parent_ptr, consumer_struct->control_node, current_data_node);
        push_stack_list(&consumer_struct->licm->pending, consumer_struct->control_node);
        consumer_struct->some_loop_invariant_moved = true;
        //Dont keep scanning this node
        return false;
    }

    //Keep scanning
    return true;
}

//Only expected: CONDITIONAL_JUMP, SET_STRUCT_FIELD, SET_ARRAY_ELEMENT, PRINT, SET_GLOBAL, DEFINE_SSA_NAME, DATA, RETURN
static bool is_data_node_loop_invariant(
        struct licm * licm,
        struct ssa_control_node * control_node,
        struct ssa_data_node * data_node
) {
    struct u64_set used_ssa_names = get_used_ssa_names_ssa_data_node(data_node, &licm->allocator.lox_allocator);
    struct ssa_block_loop_info * loop_info = get_loop_info_ssa_block(control_node->block, &licm->allocator.lox_allocator);

    bool all_loop_invariant = false;
    
    FOR_EACH_U64_SET_VALUE(used_ssa_names, used_ssa_name_u64) {
        struct ssa_name used_ssa_name = CREATE_SSA_NAME_FROM_U64(used_ssa_name_u64);

        if(contains_u64_set(&loop_info->modified_ssa_names, used_ssa_name.u16)) {
            all_loop_invariant = false;
            break;
        }
    }
    
    return all_loop_invariant;
}

//Moves invariant_data_node out of the loop. It will move the invariant in the previous block of the loop
static void move_up_loop_invariant(
        struct licm * licm,
        void ** parent_ptr,
        struct ssa_control_node * invariant_control_node, //Node that holds the loop invariant
        struct ssa_data_node * invariant_data_node
) {
    struct ssa_name invariant_name = alloc_ssa_name_ssa_ir(licm->ssa_ir, 1, "loopInvariant");
    struct ssa_block * loop_condition = invariant_control_node->block->loop_condition_block;
    struct ssa_block * block_to_move_invariant = get_block_to_move_invariant(licm, loop_condition);

    struct ssa_control_define_ssa_name_node * define_loop_invariant = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME,
            struct ssa_control_define_ssa_name_node, block_to_move_invariant, SSA_IR_NODE_LOX_ALLOCATOR(licm->ssa_ir));
    define_loop_invariant->ssa_name = invariant_name;
    define_loop_invariant->value = invariant_data_node;
    add_last_control_node_ssa_block(block_to_move_invariant, &define_loop_invariant->control);
    put_u64_hash_table(&licm->ssa_ir->ssa_definitions_by_ssa_name, invariant_name.u16, define_loop_invariant);

    struct ssa_data_get_ssa_name_node * get_loop_invariant = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_GET_SSA_NAME,
            struct ssa_data_get_ssa_name_node, NULL, SSA_IR_NODE_LOX_ALLOCATOR(licm->ssa_ir));
    get_loop_invariant->ssa_name = invariant_name;
    add_ssa_name_use_ssa_ir(licm->ssa_ir, invariant_name, invariant_control_node);
    *parent_ptr = &get_loop_invariant->data;
}

static struct ssa_block * get_block_to_move_invariant(struct licm * licm, struct ssa_block * loop_condition_block) {
    struct ssa_block * first_predecessor_of_loop_condition = (struct ssa_block *) get_first_value_u64_set(loop_condition_block->predecesors);
    bool create_new_block = size_u64_set(loop_condition_block->predecesors) > 1 || first_predecessor_of_loop_condition->is_loop_condition;
    if (!create_new_block) {
        return first_predecessor_of_loop_condition;
    }

    struct ssa_block * new_block = alloc_ssa_block(SSA_IR_NODE_LOX_ALLOCATOR(licm->ssa_ir));
    new_block->predecesors = clone_u64_set(&loop_condition_block->predecesors, SSA_IR_NODE_LOX_ALLOCATOR(licm->ssa_ir));
    new_block->ssa_ir_head_block = loop_condition_block->ssa_ir_head_block;
    new_block->type_next_ssa_block = TYPE_NEXT_SSA_BLOCK_SEQ;
    new_block->next_as.next = loop_condition_block;
    new_block->nested_loop_body = MAX(first_predecessor_of_loop_condition->nested_loop_body - 1, 0);
    new_block->loop_condition_block = loop_condition_block;

    clear_u64_set(&loop_condition_block->predecesors);
    add_u64_set(&loop_condition_block->predecesors, (uint64_t) new_block);

    FOR_EACH_U64_SET_VALUE(new_block->predecesors, predecessor_new_block_u64_ptr) {
        struct ssa_block * predecessor_new_block = (struct ssa_block *) predecessor_new_block_u64_ptr;
        predecessor_new_block->type_next_ssa_block = TYPE_NEXT_SSA_BLOCK_SEQ;
        predecessor_new_block->next_as.next = new_block;
    }

    return new_block;
}

static bool can_control_node_be_checked_for_loop_invariant(struct ssa_control_node * node) {
    if(node->block->is_loop_condition){
        return false;
    }

    switch (node->type) {
        case SSA_CONTROL_NODE_TYPE_DATA:
        case SSA_CONTROL_NODE_TYPE_RETURN:
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD:
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT:
        case SSA_CONTROL_NODE_TYPE_PRINT:
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME:
            return true;
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        default:
            return false;
    }
}

struct licm * alloc_loop_invariant_code_motion(struct ssa_ir * ssa_ir) {
    struct licm * licm = NATIVE_LOX_MALLOC(sizeof(struct licm));
    struct arena arena;
    init_arena(&arena);
    licm->ssa_ir = ssa_ir;
    licm->allocator = to_lox_allocator_arena(arena);
    init_stack_list(&licm->pending, &licm->allocator.lox_allocator);
    return licm;
}

void free_loop_invariant_code_motion(struct licm * licm) {
    free_arena(&licm->allocator.arena);
    NATIVE_LOX_FREE(licm);
}
