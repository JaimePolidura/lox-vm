#include "strength_reduction.h"

struct sr {
    struct arena_lox_allocator sr_allocator;
    struct ssa_ir * ssa_ir;
};

static struct sr alloc_strength_reduction(struct ssa_ir *);
static void free_strength_reduction(struct sr *);
static void perform_strength_reduction_block(struct ssa_block *, void *);
static bool perform_strength_reduction_data_node(struct ssa_data_node *, void **, struct ssa_data_node *, void *);

typedef void (*strength_reduction_transformer_t) (struct ssa_data_binary_node *, void **, struct sr *);
static void modulo_strength_reduction_transformer(struct ssa_data_binary_node *, void **,struct sr *);

#define SSA_NODES_ALLOCATOR(sr) (&(sr)->ssa_ir->ssa_nodes_allocator_arena.lox_allocator)

strength_reduction_transformer_t transformation_by_binary_op[] = {
        [OP_ADD] = NULL,
        [OP_SUB] = NULL,
        [OP_MUL] = NULL,
        [OP_DIV] = NULL,
        [OP_GREATER] = NULL,
        [OP_BINARY_OP_AND] = NULL,
        [OP_BINARY_OP_OR] = NULL,
        [OP_LEFT_SHIFT] = NULL,
        [OP_RIGHT_SHIFT] = NULL,
        [OP_LESS] = NULL,
        [OP_MODULO] = &modulo_strength_reduction_transformer,
        [OP_EQUAL] = NULL,
};

void perform_strength_reduction(struct ssa_ir *ssa_ir) {
    struct sr strength_reduction = alloc_strength_reduction(ssa_ir);

    for_each_ssa_block(
            ssa_ir->first_block,
            &strength_reduction.sr_allocator.lox_allocator,
            &strength_reduction,
            perform_strength_reduction_block
    );

    free_strength_reduction(&strength_reduction);
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
    struct sr * sr = extra;

    if (current_data_node->type == SSA_DATA_NODE_TYPE_BINARY) {
        struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) current_data_node;
        strength_reduction_transformer_t transformer = transformation_by_binary_op[binary->operand];
        if (transformer != NULL) {
            transformer(binary, parent_ptr, sr);
        }
    }

    return true;
}

static void modulo_strength_reduction_transformer(
        struct ssa_data_binary_node * binary_op,
        void ** parent_ptr,
        struct sr * sr
) {
    // number % k -> Where k is power of 2: (number - 1) & k
    if (binary_op->right == OP_CONSTANT) {
        double constant_right = AS_NUMBER(GET_CONST_VALUE_SSA_NODE(binary_op->right));
        if (is_double_power_of_2(constant_right)) {
            binary_op->operand = OP_BINARY_OP_AND;

            struct ssa_data_binary_node * new_left = ALLOC_SSA_DATA_NODE(
                    SSA_DATA_NODE_TYPE_BINARY, struct ssa_data_binary_node, NULL, SSA_NODES_ALLOCATOR(sr)
            );
            new_left->operand = OP_SUB;
            new_left->left = binary_op->left;
            new_left->right = &create_ssa_const_node(TO_LOX_VALUE_NUMBER(1), NULL, SSA_NODES_ALLOCATOR(sr))->data;

            binary_op->left = &new_left->data;
        }
    }
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