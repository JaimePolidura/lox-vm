#pragma once

#include "runtime/jit/x64/opcodes.h"
#include "runtime/jit/x64/x64_stack.h"

#include "shared/types/function_object.h"

uint16_t call_dynamic_external_c_function(
        struct function_object * compiling_function,
        struct u8_arraylist * native_code,
        struct operand function_address,
        int n_arguments,
        ... //Operands
);

uint16_t call_external_c_function(
        struct function_object * compiling_function,
        struct u8_arraylist * native_code,
        uint64_t function_address,
        int n_arguments,
        ... //Operands
        );
