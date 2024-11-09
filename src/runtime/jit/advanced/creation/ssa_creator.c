#include "ssa_creator.h"

extern struct ssa_control_node * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
);

struct ssa_control_node * create_ssa_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
) {
    struct ssa_control_node * ssa = create_ssa_ir_no_phis(package, function, start_function_bytecode);
    return ssa;
}