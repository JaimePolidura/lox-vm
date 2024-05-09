#include "function_calls.h"
#include "x64_jit_compiler.h"

extern struct jit_mode_switch_info switch_jit_to_native_mode(struct jit_compiler *);
extern struct jit_mode_switch_info switch_native_to_jit_mode(struct jit_compiler *);
extern struct jit_mode_switch_info switch_jit_to_vm_gc_mode(struct jit_compiler *);
extern struct jit_mode_switch_info switch_vm_gc_to_jit_mode(struct jit_compiler *, struct jit_mode_switch_info);

extern void runtime_panic(char * format, ...);

static void restore_caller_registers (
        struct u8_arraylist * native_code,
        int n_args
);

static void load_arguments_into_registers(
    struct u8_arraylist * native_code,
    struct operand * arguments,
    int n_operands
);

static void save_caller_registers(
    struct u8_arraylist * native_code,
    int n_args,
    struct operand function_ptr
);

static struct jit_mode_switch_info switch_to_function_mode(struct jit_compiler *, mode_t new_mode, int mode_switch_config);
static void switch_to_prev_mode(struct jit_compiler *, struct jit_mode_switch_info, mode_t prev_mode, int mode_switch_config);

uint16_t call_external_c_function(
        struct jit_compiler * jit_compiler,
        jit_mode_t function_mode,
        int mode_switch_config,
        struct operand function_address,
        int n_arguments,
        ... //Arguments
) {
    struct function_object * compiling_function = jit_compiler->function_to_compile;
    struct u8_arraylist * native_code = &jit_compiler->native_compiled_code;
    struct operand arguments[n_arguments];
    VARARGS_TO_ARRAY(struct operand, arguments, n_arguments, ...);
    uint16_t instruction_index = native_code->in_use;
    mode_t prev_mode = jit_compiler->current_mode;

    struct jit_mode_switch_info jit_mode_switch_info =
            switch_to_function_mode(jit_compiler, function_mode, mode_switch_config);

    save_caller_registers(native_code, n_arguments, function_address);

    load_arguments_into_registers(native_code, arguments, n_arguments);

    emit_call(native_code, R10_REGISTER_OPERAND);

    restore_caller_registers(native_code, n_arguments);

    switch_to_prev_mode(jit_compiler, jit_mode_switch_info, prev_mode, mode_switch_config);

    return instruction_index;
}

static void load_arguments_into_registers(
        struct u8_arraylist * native_code,
        struct operand * arguments,
        int n_operands
) {
    for(int i = 0; i < n_operands; i++){
        register_t argument_to_push = args_call_convention[i];
        emit_mov(native_code, REGISTER_TO_OPERAND(argument_to_push), arguments[i]);
    }

#ifdef __WIN32
    //On Windows x64 ABI, functions always place its arguments in the stack despite they are only passed by registers.
    //The caller must make sure to accomodate enough space in the stack, so that the callee is able to use it.
    //As we only support 4 arguments, 0x20 (4 words) is enough
    emit_sub(native_code, RSP_REGISTER_OPERAND, IMMEDIATE_TO_OPERAND(0x20));
#endif
}

static void save_caller_registers(struct u8_arraylist * native_code,
        int n_args, struct operand function_ptr) {
    emit_push(native_code, R10_REGISTER_OPERAND);
    emit_mov(native_code, R10_REGISTER_OPERAND, function_ptr);

    //Save args registers
    for(int i = 0; i < n_args; i++) {
        register_t argument_to_push = args_call_convention[i];
        emit_push(native_code, REGISTER_TO_OPERAND(argument_to_push));
    }

    //Save caller save registers
    for(int i = 0; i < sizeof(caller_saved_registers) / sizeof(register_t); i++){
        register_t argument_to_push = caller_saved_registers[i];
        emit_push(native_code, REGISTER_TO_OPERAND(argument_to_push));
    }
}

static void restore_caller_registers(
        struct u8_arraylist * native_code,
        int n_args
) {
#ifdef __WIN32
    emit_add(native_code, RSP_REGISTER_OPERAND, IMMEDIATE_TO_OPERAND(0x20));
#endif

    //Restore caller save registers
    for(int i = sizeof(caller_saved_registers) / sizeof(register_t) - 1; i >= 0; i--){
        register_t argument_to_pop = caller_saved_registers[i];
        emit_pop(native_code, REGISTER_TO_OPERAND(argument_to_pop));
    }

    //Restore arg registers
    for(int i = n_args - 1; i >= 0; i--){
        register_t argument_to_pop = args_call_convention[i];
        emit_pop(native_code, REGISTER_TO_OPERAND(argument_to_pop));
    }

    emit_pop(native_code, R10_REGISTER_OPERAND);
}

static struct jit_mode_switch_info switch_to_function_mode(struct jit_compiler * jit_compiler, mode_t new_mode, int mode_switch_config) {
    if(jit_compiler->current_mode == new_mode || mode_switch_config == DONT_SWITCH_MODES){
        return  NO_MODE_SWITCH_INFO;
    }

    if(jit_compiler->current_mode == MODE_JIT && new_mode == MODE_VM_GC) {
        return switch_jit_to_vm_gc_mode(jit_compiler);
    } else {
        runtime_panic("Illegal JIT mode transition. from %i to %i", jit_compiler->current_mode, new_mode);
    }

    //Unreachable code
    return NO_MODE_SWITCH_INFO;
}

static void switch_to_prev_mode(
        struct jit_compiler * jit_compiler,
        struct jit_mode_switch_info jit_mode_switch_info,
        mode_t prev_mode,
        int mode_switch_config
) {
    if(jit_compiler->current_mode == prev_mode ||
        mode_switch_config == DONT_SWITCH_MODES ||
        mode_switch_config == KEEP_MODE_AFTER_CALL){
        return;
    }

    if(jit_compiler->current_mode == MODE_VM_GC && prev_mode == MODE_JIT) {
        switch_vm_gc_to_jit_mode(jit_compiler, jit_mode_switch_info);
    } else {
        runtime_panic("Illegal JIT mode transition. from %i to %i", jit_compiler->current_mode, prev_mode);
    }
}