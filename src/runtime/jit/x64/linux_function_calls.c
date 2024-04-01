#include "function_calls.h"

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
    uint64_t function_ptr,
    uint16_t * instruction_index
);

//We use R9 to store the function address
static register_t linux_args_call_convention[] = {
        RDI,
        RSI,
        RDX,
        RCX,
        R8
};

uint16_t call_external_c_function(
        struct u8_arraylist * native_code,
        uint64_t function_address,
        int n_arguments,
        ... //Arguments
) {
    struct operand arguments[n_arguments];
    VARARGS_TO_ARRAY(struct operand, arguments, n_arguments, ...);
    uint16_t instruction_index = 0;

    save_caller_registers(native_code, n_arguments, function_address, &instruction_index);

    load_arguments_into_registers(native_code, arguments, n_arguments);

    emit_call(native_code, R9_OPERAND);

    restore_caller_registers(native_code, n_arguments);

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
        int n_operands, uint64_t function_ptr, uint16_t * instruction_index) {

    *instruction_index = emit_push(native_code, R9_OPERAND);
    emit_mov(native_code, R9_OPERAND, IMMEDIATE_TO_OPERAND(function_ptr));

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

    emit_pop(native_code, R9_OPERAND);
}