#include "lox_ir_creator.h"

static struct arena_lox_allocator * create_arena_lox_allocator();

struct lox_ir create_lox_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode,
        long options
) {
    struct arena_lox_allocator * nodes_allocator = create_arena_lox_allocator();

    struct lox_ir_block * first_block = create_lox_ir_no_phis(package, function, start_function_bytecode, nodes_allocator, options);
    struct phi_insertion_result phi_insertion_result = insert_lox_ir_phis(first_block, nodes_allocator);
    struct phi_optimization_result optimization_result = optimize_lox_ir_phis(first_block, &phi_insertion_result, nodes_allocator);

    struct lox_ir lox_ir = (struct lox_ir) {
        .max_version_allocated_per_local = phi_insertion_result.max_version_allocated_per_local,
        .definitions_by_ssa_name = phi_insertion_result.ssa_definitions_by_ssa_name,
        .node_uses_by_ssa_name = optimization_result.node_uses_by_ssa_name,
        .nodes_allocator_arena = nodes_allocator,
        .first_block = first_block,
        .function = function,
    };

    init_u64_hash_table(&lox_ir.definitions_by_v_reg, &nodes_allocator->lox_allocator);
    init_u64_hash_table(&lox_ir.node_uses_by_v_reg, &nodes_allocator->lox_allocator);

    return lox_ir;
}

static struct arena_lox_allocator * create_arena_lox_allocator() {
    struct arena arena;
    init_arena(&arena);

    return alloc_lox_allocator_arena(arena, NATIVE_LOX_ALLOCATOR());
}