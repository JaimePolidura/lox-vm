#include "x64_jit_compiler_modes.h"

void setup_vm_to_jit_mode(struct jit_compiler * jit_compiler) {
    struct u8_arraylist * code = &jit_compiler->native_compiled_code;

    //Save previous rsp and rbp
    emit_mov(code, RCX_REGISTER_OPERAND, RSP_REGISTER_OPERAND);
    emit_mov(code, RDX_REGISTER_OPERAND, RBP_REGISTER_OPERAND);

    //Load vm_thread esp into rsp
    emit_mov(code, RSP_REGISTER_OPERAND, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    //Load slots/frame pointer to rbp
    emit_mov(code, RBP_REGISTER_OPERAND, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    //Really similar to setup_call_frame_function in vm.c
    if(jit_compiler->function_to_compile->n_arguments > 0){
        emit_sub(code, RBP_REGISTER_OPERAND, IMMEDIATE_TO_OPERAND(jit_compiler->function_to_compile->n_arguments));
    }

    emit_dec(code, RBP_REGISTER_OPERAND);

    jit_compiler->current_mode = MODE_JIT;
}

void switch_jit_to_native_mode(struct jit_compiler * jit_compiler) {
    register_t runtime_info_addr_reg = push_register_allocator(&jit_compiler->register_allocator);

    //Load x64_jit_runtime_info into runtime_info_addr_reg
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(runtime_info_addr_reg),
             REGISTER_TO_OPERAND(SELF_THREAD_ADDR_REG));
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(runtime_info_addr_reg),
             DISPLACEMENT_TO_OPERAND(runtime_info_addr_reg, offsetof(struct vm_thread, jit_runtime_info)));

    //Store in runtime_info_addr_reg rsp, rbp & self-thread pointer
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(runtime_info_addr_reg, offsetof(struct x64_jit_runtime_info, rsp)),
             RSP_REGISTER_OPERAND);
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(runtime_info_addr_reg, offsetof(struct x64_jit_runtime_info, rsp)),
             RBP_REGISTER_OPERAND);
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(runtime_info_addr_reg, offsetof(struct x64_jit_runtime_info, self_thread)),
             REGISTER_TO_OPERAND(SELF_THREAD_ADDR_REG));

    //Restore prev native rsp & rbp
    emit_mov(&jit_compiler->native_compiled_code, RSP_REGISTER_OPERAND, RCX_REGISTER_OPERAND);
    emit_mov(&jit_compiler->native_compiled_code, RBP_REGISTER_OPERAND, RDX_REGISTER_OPERAND);

    emit_push(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(runtime_info_addr_reg));

    //Deallocate runtime_info_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);

    jit_compiler->current_mode = MODE_NATIVE;
}

void switch_native_to_jit_mode(struct jit_compiler * jit_compiler) {
    register_t runtime_info_addr_reg = push_register_allocator(&jit_compiler->register_allocator);

    //Store native rsp & rbp
    emit_mov(&jit_compiler->native_compiled_code, RCX_REGISTER_OPERAND, RSP_REGISTER_OPERAND);
    emit_mov(&jit_compiler->native_compiled_code, RBX_REGISTER_OPERAND, RBP_REGISTER_OPERAND);

    //Load runtime information from stack and put it into runtime_info_addr_reg
    emit_pop(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(runtime_info_addr_reg));

    //Store back jit rsp, rbp & self-thread pointer
    emit_mov(&jit_compiler->native_compiled_code,
             RSP_REGISTER_OPERAND,
             DISPLACEMENT_TO_OPERAND(runtime_info_addr_reg, offsetof(struct x64_jit_runtime_info, rsp)));
    emit_mov(&jit_compiler->native_compiled_code,
             RBP_REGISTER_OPERAND,
             DISPLACEMENT_TO_OPERAND(runtime_info_addr_reg, offsetof(struct x64_jit_runtime_info, rbp)));
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(SELF_THREAD_ADDR_REG),
             DISPLACEMENT_TO_OPERAND(runtime_info_addr_reg, offsetof(struct x64_jit_runtime_info, self_thread)));

    pop_register_allocator(&jit_compiler->register_allocator);

    jit_compiler->current_mode = MODE_JIT;
}

void switch_vm_to_jit_mode(struct jit_compiler * jit_compiler) {
    if(jit_compiler->register_allocator.n_allocated_registers == 0){
        return;
    }

    register_t esp_addr_reg = push_register_allocator(&jit_compiler->register_allocator);

    //Save esp vm address into esp_addr_reg
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(esp_addr_reg),
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    emit_sub(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(esp_addr_reg),
             IMMEDIATE_TO_OPERAND(jit_compiler->register_allocator.n_allocated_registers));

    //Update updated esp vm value
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
             REGISTER_TO_OPERAND(esp_addr_reg));

    pop_register_allocator(&jit_compiler->register_allocator);
}