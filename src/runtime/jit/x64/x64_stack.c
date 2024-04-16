#include "x64_stack.h"

void emit_prologue_x64_stack(struct u8_arraylist * code, struct function_object * function) {
    emit_push(code, RBP_REGISTER_OPERAND);
    emit_mov(code, RBP_REGISTER_OPERAND, RSP_REGISTER_OPERAND);
}

void emit_epilogue_x64_stack(struct u8_arraylist * code, struct function_object * function) {
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

    //Really similar to setup_call_frame_function in vm.c

    if(function->n_arguments > 0){
        emit_sub(code, RBP_REGISTER_OPERAND, IMMEDIATE_TO_OPERAND(function->n_arguments));
    }

    emit_dec(code, RBP_REGISTER_OPERAND);
}

void switch_jit_to_native_mode(struct u8_arraylist * code, struct function_object * function) {
    //Store back RSP into self_thread esp
    emit_mov(code, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)), RSP_REGISTER_OPERAND);

    //Restore previous rsp & rbp
    emit_mov(code, RSP_REGISTER_OPERAND, RCX_REGISTER_OPERAND);
    emit_mov(code, RBP_REGISTER_OPERAND, RDX_REGISTER_OPERAND);
}