#pragma once

#include "opcodes.h"

uint16_t call_external_c_function(
        struct u8_arraylist * native_code,
        uint64_t * function_address,
        int n_arguments,
        ... //Operands
        );
