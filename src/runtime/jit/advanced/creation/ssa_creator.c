#include "ssa_creator.h"

extern struct ssa_block * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
);
extern void optimize_ssa_ir_phis(struct ssa_block * start_block);

//Given a byetcode_list this function will create the ssa graph ir. The graph IR consists of:
//  - Data flow graph (expressions) Represented by ssa_data_node.
//  - Control flow graph (statements) Represented by ssa_control_node.
//  - Block graph. Series of control flow graph nodes, that are run sequentally. Represented by ssa_block
// This proccess consists of these phases:
// - 1º: create_ssa_ir_no_phis()
// - 2º: create_ssa_ir_blocks()
// - 3º: insert_ssa_ir_phis()
// - 4º: optimize_ssa_ir_phis()
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