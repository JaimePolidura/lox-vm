#include "utils.h"

int get_offset_field_struct_definition_ll_lox_ir(
        struct struct_definition_object * definition,
        char * field_name
) {
    uint16_t current_offset = 0;

    for (int i = 0; i < definition->n_fields; i++, current_offset += 8) {
        struct string_object * current_field_name = definition->field_names[i];

        if (strcmp(current_field_name->chars, field_name) == 0) {
            return current_offset;
        }
    }

    return -1;
}

void emit_function_call_ll_lox_ir(
        struct lllil * lllil,
        struct lox_ir_control_node * control_node,
        void * function_address,
        int n_args,
        ... //Arguments
) {
    struct lox_allocator * allocator = &lllil->lox_ir->nodes_allocator_arena->lox_allocator;
    struct lox_ir_block * block = control_node->block;

    struct lox_ir_control_ll_function_call * func_call = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL, struct lox_ir_control_ll_function_call, block, allocator
    );
    func_call->function_call_address = function_address;
    init_ptr_arraylist(&func_call->arguments, allocator);
    resize_ptr_arraylist(&func_call->arguments, n_args);

    struct lox_ir_ll_operand arguments[n_args];
    VARARGS_TO_ARRAY(struct lox_ir_ll_operand, arguments, n_args, ...);

    for (int i = 0; i < n_args; i++) {
        struct lox_ir_ll_operand argument = arguments[i];
        struct lox_ir_ll_operand * argument_ptr = LOX_MALLOC(allocator, sizeof(argument));
        *argument_ptr = argument;

        append_ptr_arraylist(&func_call->arguments, argument_ptr);
    }

    replace_control_node_lox_ir_block(block, control_node, &func_call->node);
}