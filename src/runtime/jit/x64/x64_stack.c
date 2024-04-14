#include "x64_stack.h"

void setup_x64_stack(struct u8_arraylist * code, struct function_object * function) {
    emit_push(code, RBP_REGISTER_OPERAND);
    emit_mov(code, RBP_REGISTER_OPERAND, RSP_REGISTER_OPERAND);
    switch_to_lox_stack(code, function);
}

void end_x64_stack(struct u8_arraylist * code, struct function_object * function) {
    restore_from_lox_stack(code, function);
    emit_pop(code, RBP_REGISTER_OPERAND);
    emit_ret(code);
}

void switch_to_lox_stack(struct u8_arraylist * code, struct function_object * function) {
    //Save previous rsp and rbp
    emit_mov(code, RCX_REGISTER_OPERAND, RSP_REGISTER_OPERAND);
    emit_mov(code, RDX_REGISTER_OPERAND, RBP_REGISTER_OPERAND);

    //Load vm_thread esp into rsp
    emit_mov(code, RSP_REGISTER_OPERAND, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    //Load slots/frame pointer to rbp
    emit_mov(code, RBP_REGISTER_OPERAND, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));
    emit_sub(code, RBP_REGISTER_OPERAND, IMMEDIATE_TO_OPERAND(function->n_arguments));
    emit_dec(code, RBP_REGISTER_OPERAND);
}

void restore_from_lox_stack(struct u8_arraylist * code, struct function_object * function) {
    //Store back RSP into self_thread esp
    emit_mov(code, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)), RSP_REGISTER_OPERAND);

    //Restore previous rsp & rbp
    emit_mov(code, RSP_REGISTER_OPERAND, RCX_REGISTER_OPERAND);
    emit_mov(code, RBP_REGISTER_OPERAND, RDX_REGISTER_OPERAND);
}