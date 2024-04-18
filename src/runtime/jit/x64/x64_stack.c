#include "x64_stack.h"

void emit_prologue_x64_stack(struct u8_arraylist * code, struct function_object * function) {
    emit_push(code, RBP_REGISTER_OPERAND);
    emit_mov(code, RBP_REGISTER_OPERAND, RSP_REGISTER_OPERAND);
}

void emit_epilogue_x64_stack(struct u8_arraylist * code, struct function_object * function) {
    emit_pop(code, RBP_REGISTER_OPERAND);
    emit_ret(code);
}