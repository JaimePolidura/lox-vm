#include "x64_stack.h"

void prepare_x64_stack(struct u8_arraylist * code) {
    emit_push(code, RBP_OPERAND);
    emit_mov(code, RSP_OPERAND, RBP_OPERAND);
}

void end_x64_stack(struct u8_arraylist * code) {
    emit_pop(code, RBP_OPERAND);
    emit_ret(code);
}