#include "ssa_creator.h"

extern struct ssa_block * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
);
extern void optimize_ssa_ir_phis(struct ssa_block *, struct phi_insertion_result *);

struct ssa_block * create_ssa_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
) {
    struct ssa_block * first_block = create_ssa_ir_no_phis(package, function, start_function_bytecode);
    struct phi_insertion_result phi_insertion_result = insert_ssa_ir_phis(first_block);
    optimize_ssa_ir_phis(first_block, &phi_insertion_result);

    return first_block;
}