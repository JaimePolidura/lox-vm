#include "ssa_creator.h"

extern struct ssa_block * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
);
extern void optimize_ssa_ir_phis(struct ssa_block * start_block);

struct ssa_block * create_ssa_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
) {
    struct ssa_block * first_block = create_ssa_ir_no_phis(package, function, start_function_bytecode);
    insert_ssa_ir_phis(first_block);
    optimize_ssa_ir_phis(first_block);

    return first_block;
}