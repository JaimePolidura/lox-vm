#pragma once

#include "shared.h"
#include "runtime/jit/advanced/ssa_name.h"

#define CREATE_V_REG_BY_NUMBER(n, ssa_name) ((struct v_register) {.number = (n), .register_bit_size = 64, .is_float_register = false})

struct v_register {
    bool is_float_register;
    uint16_t number;
    size_t register_bit_size;
};
