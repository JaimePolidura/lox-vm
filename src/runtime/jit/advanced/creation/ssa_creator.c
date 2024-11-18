#include "ssa_creator.h"

extern struct ssa_control_node * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
);
extern struct ssa_block * create_ssa_ir_blocks(struct ssa_control_node * start);
extern void insert_ssa_ir_phis(struct ssa_block * start_block);
extern void optimize_ssa_ir_phis(struct ssa_block * start_block);

//Given a byetcode_list this function will create the ssa graph ir. The graph IR consists of:
//  - Data flow graph (expressions) Represented by ssa_data_node.
//  - Control flow graph (statements) Represented by ssa_control_node.
//  - Block graph. Series of control flow graph nodes, that are run sequentally. Represented by ssa_block

// This proccess consists of these phases:
// - 1º: create_ssa_ir_no_phis() Creates the ssa ir, without phi functions. OP_GET_LOCAL and OP_SET_LOCAL are represented with
//    ssa_control_set_local_node and ssa_data_get_local_node.
// - 2º: create_ssa_ir_blocks() Creates the ssa ir blocks, which is a grouping of instructions without branches.
// - 3º: insert_ssa_ir_phis() Inserts phi functions. Replaces nodes ssa_control_set_local_node with ssa_control_define_ssa_name_node and
//      ssa_data_get_local_node with ssa_data_phi_node. These phase will add innecesary phi functions like: a1 = phi(a0).
// - 4º: optimize_ssa_ir_phis() Removes innecesary phi functions. Like a1 = phi(a0), it will replace it with: a1 = a0. a0 will be
//      represented with the node ssa_data_get_ssa_name_node.
struct ssa_block * create_ssa_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
) {
    struct ssa_control_node * ssa = create_ssa_ir_no_phis(package, function, start_function_bytecode);
    struct ssa_block * block = create_ssa_ir_blocks(ssa);
    insert_ssa_ir_phis(block);
    optimize_ssa_ir_phis(block);

    return block;
}