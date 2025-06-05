#include "live_range_analysis.h"

struct lra {
    struct ptr_arraylist liveranges;

    struct lox_ir * lox_ir;
    struct arena_lox_allocator lra_allocator;
};

static struct lra * alloc_live_range_analysis(struct lox_ir * lox_ir);
static void free_live_range_analysis(struct lra * lra);
static struct liverange * create_live_range(struct ssa_name, struct lox_ir_control_node*, struct lox_ir_control_ll_move*);
static bool perform_live_range_analysis_block(struct lox_ir_block * block, void * extra);
static void save_liverange(struct lra *, struct liverange *);
static void add_used_phi_ssa_names_to_pending_definitions_to_scan(struct lra*,struct lox_ir_control_node*,
        struct lox_ir_control_ll_move*,struct stack_list*);

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
            struct u64_set * v_reg_definitions = get_u64_hash_table(&lra->lox_ir->definitions_by_ssa_name, v_reg_ssa_name_u16);

            struct stack_list definitions_pending_scan;
            init_stack_list(&definitions_pending_scan, &lra->lra_allocator.lox_allocator);
            push_set_stack_list(&definitions_pending_scan, *v_reg_definitions);

            while (!is_empty_stack_list(&definitions_pending_scan)) {
                struct lox_ir_control_ll_move * definition = pop_stack_list(&definitions_pending_scan);

                if (is_define_phi_lox_ir_control(&definition->control)) {
                    add_used_phi_ssa_names_to_pending_definitions_to_scan(lra, control, definition, &definitions_pending_scan);
                } else {
                    struct liverange * live_range = create_live_range(used_ssa_name, control, definition);
                    save_liverange(lra, live_range);
                }
            }
        }
    }
}

static void add_used_phi_ssa_names_to_pending_definitions_to_scan(
        struct lra * lra,
        struct lox_ir_control_node * use,
        struct lox_ir_control_ll_move * phi_definition,
        struct stack_list * pending_definitions_to_scan
) {
    struct u64_set used_ssa_names_in_phi = get_names_defined_phi_lox_ir_control(phi_definition,
                                                                                &lra->lra_allocator.lox_allocator);

    FOR_EACH_U64_SET_VALUE(used_ssa_names_in_phi, uint16_t, used_ssa_name_in_phi_u16) {
        struct ssa_name used_ssa_name_in_phi = CREATE_SSA_NAME_FROM_U64(used_ssa_name_in_phi_u16);
        //Function argument
        if (used_ssa_name_in_phi.value.version == 0) {
            struct liverange * live_range = create_live_range(used_ssa_name_in_phi, use, NULL);
            save_liverange(lra, live_range);
        } else {
            struct lox_ir_control_node * definition_of_used_ssa_name_in_phi = get_u64_hash_table(
                    &lra->lox_ir->definitions_by_ssa_name, used_ssa_name_in_phi.u16);
            push_stack_list(pending_definitions_to_scan, definition_of_used_ssa_name_in_phi);
        }
    }
}

static void save_liverange(struct lra * lra, struct liverange * liverange) {
    //TODO Check if range intersects with other
}

static struct liverange * create_live_range(
        struct ssa_name v_reg_ssa_name,
        struct lox_ir_control_node * use,
        //Can be NULL
        struct lox_ir_control_ll_move * definition
) {
    struct lox_ir_block * definition_block = definition->control.block;
    int definition_block_index = get_index_control_lox_ir_block(definition_block, (struct lox_ir_control_node *) definition);
    int use_block_index = get_index_control_lox_ir_block(use->block, use);

    struct liverange * liverange = NULL;

    if (definition != NULL) {
        liverange->from_control_node_index = definition_block_index;
        liverange->from_block = definition_block;
        liverange->from_control_node = definition;
    } else {
        liverange->from_block = NULL;
        liverange->from_control_node = NULL;
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