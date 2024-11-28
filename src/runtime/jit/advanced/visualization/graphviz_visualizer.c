#include "graphviz_visualizer.h"

static int create_graph_and_write(struct ssa_block *);

int generate_ssa_graphviz_graph(
        struct package * package,
        struct function_object * function,
        phase_ssa_graphviz_t phase,
        char * path
) {
    struct arena arena;
    init_arena(&arena);
    struct arena_lox_allocator ssa_node_allocator = to_lox_allocator_arena(arena);

    switch (phase) {
        case NO_PHIS_PHASE_SSA_GRAPHVIZ: {
            struct ssa_block * ssa_block = create_ssa_ir_no_phis(package, function,
                create_bytecode_list(function->chunk, &ssa_node_allocator.lox_allocator), &ssa_node_allocator);

            return create_graph_and_write(ssa_block);
        }
        case PHIS_INSERTED_PHASE_SSA_GRAPHVIZ: {
            struct ssa_block * ssa_block = create_ssa_ir_no_phis(package, function,
                create_bytecode_list(function->chunk, &ssa_node_allocator.lox_allocator), &ssa_node_allocator);
            insert_ssa_ir_phis(ssa_block, &ssa_node_allocator);

            return create_graph_and_write(ssa_block);
        }
        case PHIS_OPTIMIZED_PHASE_SSA_GRAPHVIZ: {
            struct ssa_block * ssa_block = create_ssa_ir_no_phis(package, function,
                create_bytecode_list(function->chunk, &ssa_node_allocator.lox_allocator), &ssa_node_allocator);
            struct phi_insertion_result phi_insertion_result = insert_ssa_ir_phis(ssa_block, &ssa_node_allocator);
            optimize_ssa_ir_phis(ssa_block, &phi_insertion_result, &ssa_node_allocator);

            return create_graph_and_write(ssa_block);
        }
        case SPARSE_CONSTANT_PROPAGATION_PHASE_SSA_GRAPHVIZ: {
            struct ssa_ir ssa_ir = create_ssa_ir(package, function, create_bytecode_list(function->chunk,
                &ssa_node_allocator.lox_allocator));
            perform_sparse_constant_propagation(&ssa_ir);

            return create_graph_and_write(ssa_ir.first_block);
        }
        case ALL_PHASE_SSA_GRAPHVIZ: {
            struct ssa_ir ssa_ir = create_ssa_ir(package, function, create_bytecode_list(function->chunk,
                &ssa_node_allocator.lox_allocator));
            perform_sparse_constant_propagation(&ssa_ir);

            return create_graph_and_write(ssa_ir.first_block);
        }
    }
}

static int create_graph_and_write(struct ssa_block * start_block) {

}