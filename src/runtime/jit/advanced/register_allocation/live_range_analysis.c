#include "live_range_analysis.h"

struct lra {
    struct ptr_arraylist liveranges;

    struct lox_ir * lox_ir;
    struct arena_lox_allocator lra_allocator;
};

static struct lra * alloc_live_range_analysis(struct lox_ir * lox_ir);
static void free_live_range_analysis(struct lra * lra);
static struct liverange * create_live_range(int v_reg, struct lox_ir_control_node*, struct lox_ir_control_node*);
static bool perform_live_range_analysis_block(struct lox_ir_block * block, void * extra);
static void save_liverange(struct lra *, struct liverange *);

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
        struct u64_set used_v_regs = get_used_v_registers_lox_ir_control(control, NATIVE_LOX_ALLOCATOR());

        FOR_EACH_U64_SET_VALUE(used_v_regs, int, v_reg_num) {
            struct u64_set * v_reg_definitions = get_u64_hash_table(&lra->lox_ir->definitions_by_v_reg, v_reg_num);

            FOR_EACH_U64_SET_VALUE(*v_reg_definitions, struct lox_ir_control_node *, definition) {
                struct liverange * live_range = create_live_range(v_reg_num, control, definition);
                save_liverange(lra, live_range);
            }
        }
    }
}

static void save_liverange(struct lra * lra, struct liverange * liverange) {
    //TODO Check if range intersects with other
}

static struct liverange * create_live_range(
        int v_reg,
        struct lox_ir_control_node * use,
        struct lox_ir_control_node * definition
) {
    int definition_block_index = get_index_control_lox_ir_block(definition->block, definition);
    int use_block_index = get_index_control_lox_ir_block(use->block, use);

    struct liverange * liverange = NULL;

    liverange->from_control_node_index = definition_block_index;
    liverange->from_block = definition->block;
    liverange->from_control_node = definition;
    liverange->to_control_node_index = use_block_index;
    liverange->to_block = use->block;
    liverange->to_control_node = use;
    liverange->v_reg_number = v_reg;

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