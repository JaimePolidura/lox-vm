#include "ssa_creator.h"

extern struct ssa_block * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode,
        struct arena_lox_allocator * ssa_node_allocator
);

static struct arena_lox_allocator * create_arena_lox_allocator();

struct ssa_ir create_ssa_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode,
        long ssa_options
) {
    struct arena_lox_allocator * ssa_nodes_allocator = create_arena_lox_allocator();

    struct ssa_block * first_block = create_ssa_ir_no_phis(package, function, start_function_bytecode, ssa_nodes_allocator);
    struct phi_insertion_result phi_insertion_result = insert_ssa_ir_phis(first_block, ssa_nodes_allocator);
    struct phi_optimization_result optimization_result = optimize_ssa_ir_phis(first_block, &phi_insertion_result, ssa_nodes_allocator);

    return (struct ssa_ir) {
        .max_version_allocated_per_local = phi_insertion_result.max_version_allocated_per_local,
        .ssa_definitions_by_ssa_name = phi_insertion_result.ssa_definitions_by_ssa_name,
        .node_uses_by_ssa_name = optimization_result.node_uses_by_ssa_name,
        .ssa_nodes_allocator_arena = ssa_nodes_allocator,
        .first_block = first_block,
        .function = function,
    };
}

static struct arena_lox_allocator * create_arena_lox_allocator() {
    struct arena arena;
    init_arena(&arena);

    return alloc_lox_allocator_arena(arena, NATIVE_LOX_ALLOCATOR());
}