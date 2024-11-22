#include "ssa_creator.h"

extern struct ssa_block * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
);

struct ssa_ir create_ssa_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
) {
    struct ssa_block * first_block = create_ssa_ir_no_phis(package, function, start_function_bytecode);
    struct phi_insertion_result phi_insertion_result = insert_ssa_ir_phis(first_block);
    struct phi_optimization_result optimization_result = optimize_ssa_ir_phis(first_block, &phi_insertion_result);

    return (struct ssa_ir) {
        .max_version_allocated_per_local = phi_insertion_result.max_version_allocated_per_local,
        .ssa_definitions_by_ssa_name = phi_insertion_result.ssa_definitions_by_ssa_name,
        .node_uses_by_ssa_name = optimization_result.node_uses_by_ssa_name,
        .first_block = first_block,
    };
}