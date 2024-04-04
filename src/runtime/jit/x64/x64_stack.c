#include "x64_stack.h"

void prepare_x64_stack(struct u8_arraylist * code, struct function_object * function) {
    emit_push(code, RBP_OPERAND);
    emit_mov(code, RBP_OPERAND, RSP_OPERAND);

    //TODO Replace with number of locals
    if(function->n_arguments > 0) {
        emit_sub(code, RSP_OPERAND, IMMEDIATE_TO_OPERAND(sizeof(lox_value_t) * function->n_arguments));
    }
}

void end_x64_stack(struct u8_arraylist * code) {
    emit_pop(code, RBP_OPERAND);
    emit_ret(code);
}