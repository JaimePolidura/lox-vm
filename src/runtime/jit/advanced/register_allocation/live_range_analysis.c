#include "live_range_analysis.h"

struct lra {
    struct ptr_arraylist liveranges;

    struct lox_ir * lox_ir;
    struct arena_lox_allocator lra_allocator;
};

#define ARGUMENT_PASSED_NAME_DEFINITION ((void *) 0x01)

static struct lra * alloc_live_range_analysis(struct lox_ir * lox_ir);
static void free_live_range_analysis(struct lra * lra);
static struct liverange * create_live_range(struct ssa_name, struct lox_ir_control_node*, struct lox_ir_control_ll_move*);
static bool perform_live_range_analysis_block(struct lox_ir_block * block, void * extra);
static void save_liverange(struct lra *, struct liverange *);
static struct lox_ir_control_node * get_definition(struct lra *, struct ssa_name name);

void perform_live_range_analysis(struct lox_ir * lox_ir) {
    struct lra * lra = alloc_live_range_analysis(lox_ir);

    for_each_lox_ir_block(
            lox_ir->first_block,
            NATIVE_LOX_ALLOCATOR(),
            lox_ir,
            LOX_IR_BLOCK_OPT_NOT_REPEATED,
            perform_live_range_analysis_block
    );

    free_live_range_analysis(lra);
}

static bool perform_live_range_analysis_block(struct lox_ir_block * block, void * extra) {
    struct lra * lra = extra;

    int i = 0;

    for (struct lox_ir_control_node * control = block->first;;control = control->next, i++) {
        struct u64_set used_v_regs_ssa = get_used_ssa_names_lox_ir_control(control, &lra->lra_allocator.lox_allocator);

        FOR_EACH_U64_SET_VALUE(used_v_regs_ssa, uint16_t, v_reg_ssa_name_u16) {
            struct ssa_name used_ssa_name = CREATE_SSA_NAME_FROM_U64(v_reg_ssa_name_u16);

            struct lox_ir_control_node * definition = get_definition(lra, used_ssa_name);
            struct liverange * live_range = create_live_range(used_ssa_name, control, definition);
            save_liverange(lra, live_range);
        }
    }
}

static struct lox_ir_control_node * get_definition(struct lra * lra, struct ssa_name name) {
    if (name.value.version == 0) {
       return ARGUMENT_PASSED_NAME_DEFINITION;
    }

    struct lox_ir_control_node * initial_definition = get_u64_hash_table(
            &lra->lox_ir->definitions_by_ssa_name, name.u16);
    struct stack_list pending_definitions;
    init_stack_list(&pending_definitions, &lra->lra_allocator);
    push_stack_list(&pending_definitions, initial_definition);

    struct ssa_name first_ssa_name_def = CREATE_SSA_NAME(name.value.local_number, 1);

    bool is_from_arg = false;

    while (!is_empty_stack_list(&pending_definitions)) {
        struct lox_ir_control_node * definition = (struct lox_ir_control_node *)
                pop_stack_list(&pending_definitions);
        struct ssa_name * defined_ssa_name = get_defined_ssa_name_lox_ir_control(definition);

        if (defined_ssa_name->value.version == 0) {
           is_from_arg = true;
           break;
        }

        if (is_define_phi_lox_ir_control(definition)) {
            struct lox_ir_data_phi_node * phi = get_defined_phi_lox_ir_control(definition);
            FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, ssa_phi_name) {
                struct lox_ir_control_node * ssa_phi_name_def = get_u64_hash_table(&lra->lox_ir->definitions_by_ssa_name,
                        ssa_phi_name.u16);
                push_stack_list(&pending_definitions, ssa_phi_name_def);
            }
        }
    }

    return !is_from_arg ?
           get_u64_hash_table(&lra->lox_ir->definitions_by_ssa_name, first_ssa_name_def.u16) :
           ARGUMENT_PASSED_NAME_DEFINITION;
}

static void save_liverange(struct lra * lra, struct liverange * liverange) {
    //TODO Check if range intersects with other
}

static struct liverange * create_live_range(
        struct ssa_name v_reg_ssa_name,
        struct lox_ir_control_node * use,
        struct lox_ir_control_ll_move * definition
) {
    int use_block_index = get_index_control_lox_ir_block(use->block, use);

    struct liverange * liverange = NULL;

    if (definition != ARGUMENT_PASSED_NAME_DEFINITION) {
        struct lox_ir_block * definition_block = definition->control.block;
        int definition_block_index = get_index_control_lox_ir_block(definition_block,
            (struct lox_ir_control_node *) definition);
        liverange->from_control_node_index = definition_block_index;
        liverange->from_block = definition_block;
        liverange->from_control_node = definition;
        liverange->from_function_argument = false;
    } else {
        liverange->from_block = NULL;
        liverange->from_control_node = NULL;
        liverange->from_function_argument = true;
    }

    liverange->to_control_node_index = use_block_index;
    liverange->to_block = use->block;
    liverange->to_control_node = use;
    liverange->v_reg_ssa_name_local_number = v_reg_ssa_name.value.local_number;

    return liverange;
}

static struct lra * alloc_live_range_analysis(struct lox_ir * lox_ir) {
    struct lra * lra = LOX_MALLOC(NATIVE_LOX_ALLOCATOR(), sizeof(struct lra));

    struct arena arena;
    init_arena(&arena);
    lra->lox_ir = lox_ir;
    lra->lra_allocator = to_lox_allocator_arena(arena);

    return lra;
}

static void free_live_range_analysis(struct lra * lra) {
    free_arena(&lra->lra_allocator.arena);
    NATIVE_LOX_FREE(lra);
}