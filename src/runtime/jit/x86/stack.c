#include "stack.h"

void prepare_x64_stack(struct u8_arraylist * code) {
    emit_push(code, RBP_OPERAND);
    emit_mov(code, RBP_OPERAND, RSP_OPERAND);
}

void end_x64_stack(struct u8_arraylist *) {
    emit_pop(code, RBP_OPERAND);
    emit_ret(code);
}