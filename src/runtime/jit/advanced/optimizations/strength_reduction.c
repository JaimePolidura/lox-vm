#include "strength_reduction.h"

struct sr {
    struct arena_lox_allocator sr_allocator;
    struct ssa_ir * ssa_ir;
};

static struct sr alloc_strength_reduction(struct ssa_ir *);
static void free_strength_reduction(struct sr *);
static void perform_strength_reduction_block(struct ssa_block *, void *);
static bool perform_strength_reduction_data_node(struct ssa_data_node *, void **, struct ssa_data_node *, void *);

void perform_strength_reduction(struct ssa_ir *ssa_ir) {
    struct sr strength_reudction = alloc_strength_reduction(ssa_ir);

    for_each_ssa_block(
            ssa_ir->first_block,
            &strength_reudction.sr_allocator.lox_allocator,
            &strength_reudction,
            perform_strength_reduction_block
    );

    free_strength_reduction(&strength_reudction);
}

static void perform_strength_reduction_block(struct ssa_block * block, void * extra) {
    struct sr * sr = extra;

    for (struct ssa_control_node * current_control_node = block->first;; current_control_node = current_control_node->next) {
        for_each_data_node_in_control_node(
                current_control_node,
                sr,
                SSA_DATA_NODE_OPT_PRE_ORDER | SSA_DATA_NODE_OPT_RECURSIVE,
                perform_strength_reduction_data_node
        );

        if (current_control_node == block->last) {
            break;
        }
    }
}

static bool perform_strength_reduction_data_node(
        struct ssa_data_node * _,
        void ** parent_ptr,
        struct ssa_data_node * current_data_node,
        void * extra
) {
}

static struct sr alloc_strength_reduction(struct ssa_ir * ssa_ir) {
    struct arena arena;
    init_arena(&arena);

    return (struct sr) {
        .sr_allocator = to_lox_allocator_arena(arena),
        .ssa_ir = ssa_ir
    };
}

static void free_strength_reduction(struct sr * sr) {
    free_arena(&sr->sr_allocator.arena);
}