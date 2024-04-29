#include "x64_jit_compiler_modes.h"

static int reconstruct_vm_stack(struct jit_compiler *);
static void deconstruct_vm_stack(struct jit_compiler *, struct jit_mode_switch_info);

struct jit_mode_switch_info setup_vm_to_jit_mode(struct jit_compiler * jit_compiler) {
    struct u8_arraylist * code = &jit_compiler->native_compiled_code;

    //Save previous rsp and rbp
    emit_mov(code, RCX_REGISTER_OPERAND, RSP_REGISTER_OPERAND);
    emit_mov(code, RDX_REGISTER_OPERAND, RBP_REGISTER_OPERAND);

    //Load vm_thread esp into rsp
    emit_mov(code, RSP_REGISTER_OPERAND, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    //Load slots/frame pointer to rbp
    emit_mov(code, RBP_REGISTER_OPERAND, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    //Really similar to setup_call_frame_function in vm.c Setup ebp
    size_t n_bytes_adjust_rbp = sizeof(lox_value_t) + jit_compiler->function_to_compile->n_arguments * sizeof(lox_value_t);
    emit_sub(code, RBP_REGISTER_OPERAND, IMMEDIATE_TO_OPERAND(n_bytes_adjust_rbp));

    size_t n_bytes_adjust_rsp = jit_compiler->function_to_compile->n_locals - jit_compiler->function_to_compile->n_arguments;
    if(n_bytes_adjust_rsp > 0){
        n_bytes_adjust_rsp = sizeof(lox_value_type) + n_bytes_adjust_rsp * sizeof(lox_value_t);
        emit_add(code, RSP_REGISTER_OPERAND, IMMEDIATE_TO_OPERAND(n_bytes_adjust_rsp));
    }

    jit_compiler->current_mode = MODE_JIT;

    return NO_MODE_SWITCH_INFO;
}

struct jit_mode_switch_info switch_jit_to_native_mode(struct jit_compiler * jit_compiler) {
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
             DISPLACEMENT_TO_OPERAND(runtime_info_addr_reg, offsetof(struct x64_jit_runtime_info, rbp)),
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

    return NO_MODE_SWITCH_INFO;
}

struct jit_mode_switch_info switch_native_to_jit_mode(struct jit_compiler * jit_compiler) {
    register_t runtime_info_addr_reg = push_register_allocator(&jit_compiler->register_allocator);

    //Load runtime information from stack and put it into runtime_info_addr_reg
    emit_pop(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(runtime_info_addr_reg));

    //Store native rsp & rbp
    emit_mov(&jit_compiler->native_compiled_code, RCX_REGISTER_OPERAND, RSP_REGISTER_OPERAND);
    emit_mov(&jit_compiler->native_compiled_code, RDX_REGISTER_OPERAND, RBP_REGISTER_OPERAND);

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

    return NO_MODE_SWITCH_INFO;
}

struct jit_mode_switch_info switch_jit_to_vm_mode(struct jit_compiler * jit_compiler) {
    int stack_grow = reconstruct_vm_stack(jit_compiler);
    switch_jit_to_native_mode(jit_compiler);
    jit_compiler->current_mode = MODE_VM;

    return (struct jit_mode_switch_info) {
        .as = {.jit_to_vm = {.stack_grow = stack_grow}}
    };
}

struct jit_mode_switch_info switch_vm_to_jit_mode(struct jit_compiler * jit_compiler, struct jit_mode_switch_info jit_mode_switch_info) {
    switch_native_to_jit_mode(jit_compiler);
    deconstruct_vm_stack(jit_compiler, jit_mode_switch_info);
    jit_compiler->current_mode = MODE_JIT;
    return NO_MODE_SWITCH_INFO;
}

static int reconstruct_vm_stack(struct jit_compiler * jit_compiler) {
    if(jit_compiler->register_allocator.n_allocated_registers == 0){
        return 0;
    }

    register_t esp_addr_reg = push_register_allocator(&jit_compiler->register_allocator);
    int n_allocated_registers = jit_compiler->register_allocator.n_allocated_registers;

    //Save esp vm address into esp_addr_reg
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(esp_addr_reg),
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    //We have to reconstruct the vm stack
    for(int i = n_allocated_registers - 1; i > 0; i--) {
        register_t stack_value_reg = peek_at_register_allocator(&jit_compiler->register_allocator, i);

        emit_mov(&jit_compiler->native_compiled_code,
                 DISPLACEMENT_TO_OPERAND(esp_addr_reg, 0),
                 REGISTER_TO_OPERAND(stack_value_reg));

        emit_add(&jit_compiler->native_compiled_code,
                 REGISTER_TO_OPERAND(esp_addr_reg),
                 IMMEDIATE_TO_OPERAND(sizeof(lox_value_t)));
    }

    //Update updated esp vm value
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
             REGISTER_TO_OPERAND(esp_addr_reg));

    //dealloc esp_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);

    return n_allocated_registers - 1;
}

//Avoid stack overflow if switch_jit_to_vm_mode is called multiple times
static void deconstruct_vm_stack(struct jit_compiler * jit_compiler, struct jit_mode_switch_info jit_mode_switch_info) {
    if(jit_mode_switch_info.as.jit_to_vm.stack_grow > 0) {
        //Since RSP is not modified with the new vm_thread stack value (changes in reconstruct_vm_stack), we replace vm_thread esp with it
        emit_mov(&jit_compiler->native_compiled_code,
                 DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
                 RSP_REGISTER_OPERAND);
    }
}
