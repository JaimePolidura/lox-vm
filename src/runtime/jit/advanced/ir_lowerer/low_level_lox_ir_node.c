#include "low_level_lox_ir_node.h"

struct lox_level_lox_ir_node * alloc_low_level_lox_ir_node(
        low_level_lox_ir_control_node_type type,
        size_t size,
        struct lox_allocator * allocator
) {
    struct lox_level_lox_ir_node * node = LOX_MALLOC(allocator, size);
    node->type = type;
    return node;
}

struct low_level_lox_ir_function_call * create_function_call_low_level_lox_ir(
    struct lox_allocator * allocator,
    void * function_address,
    int n_args,
    ...
) {
    struct low_level_lox_ir_function_call * func_call = ALLOC_LOW_LEVEL_LOX_IR_NODE(
            LOW_LEVEL_LOX_IR_NODE_FUNCTION_CALL, struct low_level_lox_ir_function_call, allocator
    );
    func_call->function_call_address = function_address;
    init_ptr_arraylist(&func_call->arguments, allocator);
    resize_ptr_arraylist(&func_call->arguments, n_args);

    struct operand arguments[n_args];
    VARARGS_TO_ARRAY(struct operand, arguments, n_args, ...);

    for (int i = 0; i < n_args; i++) {
        struct operand argument = arguments[i];
        struct operand * argument_ptr = LOX_MALLOC(allocator, sizeof(argument));
        *argument_ptr = argument;

        append_ptr_arraylist(&func_call->arguments, argument_ptr);
    }

    return func_call;
}