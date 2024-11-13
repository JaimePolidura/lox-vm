#include "ssa_creator.h"

extern struct ssa_control_node * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
);
extern struct ssa_block * create_ssa_ir_blocks(struct ssa_control_node * start);
extern void insert_ssa_ir_phis(struct ssa_block * start_block);

struct ssa_control_node * create_ssa_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
) {
    struct ssa_control_node * ssa = create_ssa_ir_no_phis(package, function, start_function_bytecode);
    struct ssa_block * block = create_ssa_ir_blocks(ssa);
    insert_ssa_ir_phis(block);

    return ssa;
}