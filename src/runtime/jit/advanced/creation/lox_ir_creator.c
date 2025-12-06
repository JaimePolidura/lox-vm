#include "lox_ir_creator.h"

struct lox_ir * create_lox_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode,
        long options
) {
    struct lox_ir * lox_ir = alloc_lox_ir(NATIVE_LOX_ALLOCATOR(), function, package);

    create_lox_ir_no_phis(lox_ir, start_function_bytecode, options);
    insert_lox_ir_phis(lox_ir);
    optimize_lox_ir_phis(lox_ir);

    return lox_ir;
}