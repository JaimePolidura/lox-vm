#include "function_calls.h"
#include "x64_jit_compiler.h"

extern void switch_jit_to_native_mode(struct jit_compiler * jit_compiler);
extern void switch_native_to_jit_mode(struct jit_compiler * jit_compiler);
extern void runtime_panic(char * format, ...);

static void restore_caller_registers (
        struct u8_arraylist * native_code,
        int n_operands
);

static void load_arguments_into_registers(
    struct u8_arraylist * native_code,
    struct operand * arguments,
    int n_operands
);

static void save_caller_registers(
    struct u8_arraylist * native_code,
    int n_operands,
    struct operand function_ptr
);

//We use R9 to store the function address
static register_t linux_args_call_convention[] = {
        RDI,
        RSI,
        RDX,
        RCX,
        R8
};

static void switch_to_function_mode(struct jit_compiler *, mode_t new_mode);
static void switch_to_prev_mode(struct jit_compiler *, mode_t prev_mode);

uint16_t call_external_c_function(
        struct jit_compiler * jit_compiler,
        jit_mode_t function_mode,
        bool restore_mode_after_call,
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

    switch_to_function_mode(jit_compiler, function_mode);

    save_caller_registers(native_code, n_arguments, function_address);

    load_arguments_into_registers(native_code, arguments, n_arguments);

    emit_call(native_code, R9_REGISTER_OPERAND);

    restore_caller_registers(native_code, n_arguments);

    if(restore_mode_after_call){
        switch_to_prev_mode(jit_compiler, prev_mode);
    }

    return instruction_index;
}

static void load_arguments_into_registers(
        struct u8_arraylist * native_code,
        struct operand * arguments,
        int n_operands
) {
    for(int i = 0; i < n_operands; i++){
        register_t linux_argument_to_push = linux_args_call_convention[i];
        emit_mov(native_code, REGISTER_TO_OPERAND(linux_argument_to_push), arguments[i]);
    }
}

static void save_caller_registers(struct u8_arraylist * native_code,
        int n_operands, struct operand function_ptr) {

    emit_push(native_code, R9_REGISTER_OPERAND);

    emit_mov(native_code, R9_REGISTER_OPERAND, function_ptr);

    for(int i = 0; i < n_operands; i++) {
        register_t linux_argument_to_push = linux_args_call_convention[i];
        emit_push(native_code, REGISTER_TO_OPERAND(linux_argument_to_push));
    }
}

static void restore_caller_registers(
        struct u8_arraylist * native_code,
        int n_operands
) {
    for(int i = n_operands - 1; i >= 0; i--){
        register_t linux_argument_to_pop = linux_args_call_convention[i];
        emit_pop(native_code, REGISTER_TO_OPERAND(linux_argument_to_pop));
    }

    emit_pop(native_code, R9_REGISTER_OPERAND);
}

static void switch_to_function_mode(struct jit_compiler * jit_compiler, mode_t new_mode) {
    if(jit_compiler->current_mode == new_mode){
        return;
    }

    if(jit_compiler->current_mode == MODE_JIT && new_mode == MODE_VM){

    } else if(jit_compiler->current_mode == MODE_JIT && new_mode == MODE_NATIVE){
        switch_jit_to_native_mode(jit_compiler);
    } else {
        runtime_panic("Illegal JIT mode transition. from %i to %i", jit_compiler->current_mode, new_mode);
    }
}

static void switch_to_prev_mode(struct jit_compiler * jit_compiler, mode_t prev_mode) {
    if(jit_compiler->current_mode == prev_mode){
        return;
    }

    if(jit_compiler->current_mode == MODE_NATIVE && prev_mode == MODE_JIT){
        switch_native_to_jit_mode(jit_compiler);
    }
}





