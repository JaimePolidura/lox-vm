#include "x64_jit_compiler_modes.h"

extern void runtime_panic(char * format, ...);

static int reconstruct_vm_gc_stack(struct jit_compiler *jit_compiler);
static void deconstruct_vm_gc_stack(struct jit_compiler *jit_compiler, struct jit_mode_switch_info jit_mode_switch_info);

struct jit_mode_switch_info setup_vm_to_jit_mode(struct jit_compiler * jit_compiler) {
    struct u8_arraylist * code = &jit_compiler->native_compiled_code;

    //Load vm_thread esp into LOX_ESP_REG
    emit_mov(code, LOX_ESP_REG_OPERAND, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    //Load slots/frame pointer to LOX_EBP_REG
    emit_mov(code, LOX_EBP_REG_OPERAND, DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    //Really similar to setup_call_frame_function in vm.c Setup ebp
    size_t n_bytes_adjust_rbp = sizeof(lox_value_t) + jit_compiler->function_to_compile->n_arguments * sizeof(lox_value_t);
    emit_sub(code, LOX_EBP_REG_OPERAND, IMMEDIATE_TO_OPERAND(n_bytes_adjust_rbp));

    size_t n_bytes_adjust_esp = jit_compiler->function_to_compile->n_locals - jit_compiler->function_to_compile->n_arguments;
    if(n_bytes_adjust_esp > 0){
        n_bytes_adjust_esp = sizeof(lox_value_type) + n_bytes_adjust_esp * sizeof(lox_value_t);
        emit_add(code, LOX_ESP_REG_OPERAND, IMMEDIATE_TO_OPERAND(n_bytes_adjust_esp));
    }

    jit_compiler->current_mode = MODE_JIT;

    return NO_MODE_SWITCH_INFO;
}

struct jit_mode_switch_info switch_jit_to_vm_gc_mode(struct jit_compiler * jit_compiler) {
    int stack_grow = reconstruct_vm_gc_stack(jit_compiler);
    jit_compiler->current_mode = MODE_VM_GC;

    return (struct jit_mode_switch_info) {
        .as = {.jit_to_vm_gc = {.stack_grow = stack_grow}}
    };
}

struct jit_mode_switch_info switch_vm_gc_to_jit_mode(struct jit_compiler * jit_compiler, struct jit_mode_switch_info jit_mode_switch_info) {
    deconstruct_vm_gc_stack(jit_compiler, jit_mode_switch_info);
    jit_compiler->current_mode = MODE_JIT;
    return NO_MODE_SWITCH_INFO;
}

static int reconstruct_vm_gc_stack(struct jit_compiler * jit_compiler) {
    int n_heap_allocations_in_stack = n_heap_allocations_in_jit_stack(&jit_compiler->jit_stack);

    if(n_heap_allocations_in_stack == 0){
        return 0;
    }

    //Update lox vm esp to LOX_ESP_REG_OPERAND
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
             LOX_ESP_REG_OPERAND);
    //Save esp vm address into vm_esp_addr_reg
    register_t vm_esp_addr_reg = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(vm_esp_addr_reg),
             LOX_ESP_REG_OPERAND);

    struct jit_stack_item * next_current_jit_stack_item = &jit_compiler->jit_stack.items[0];
    for(int i = 0; i < n_heap_allocations_in_stack; i++){
        //Get next heap allocated stack item
        while(!(next_current_jit_stack_item++)->is_heap_allocated);

        struct jit_stack_item * current_heap_jit_stack_item = next_current_jit_stack_item - 1;

        if(current_heap_jit_stack_item->type != REGISTER_JIT_STACK_ITEM) {
            runtime_panic("Unsupported current stack item type %i while reconstructing vm stack", current_heap_jit_stack_item->type);
        }

        emit_mov(&jit_compiler->native_compiled_code,
                 DISPLACEMENT_TO_OPERAND(vm_esp_addr_reg, 0),
                 REGISTER_TO_OPERAND(current_heap_jit_stack_item->as.reg));

        emit_add(&jit_compiler->native_compiled_code,
                 REGISTER_TO_OPERAND(vm_esp_addr_reg),
                 IMMEDIATE_TO_OPERAND(sizeof(lox_value_t)));
    }

    //Update updated esp vm value
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
             REGISTER_TO_OPERAND(vm_esp_addr_reg));

    //dealloc vm_esp_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);

    return n_heap_allocations_in_stack - 1;
}

static void deconstruct_vm_gc_stack(struct jit_compiler * jit_compiler, struct jit_mode_switch_info jit_mode_switch_info) {
    if(jit_mode_switch_info.as.jit_to_vm_gc.stack_grow > 0) {
        emit_mov(&jit_compiler->native_compiled_code,
                 DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
                 LOX_ESP_REG_OPERAND);
    }
}
