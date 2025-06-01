#pragma once

#include "shared.h"
#include "runtime/jit/advanced/ssa_name.h"

#define CREATE_V_REG(ssa_name, is_fp) ((struct v_register) {.register_bit_size = 64, .is_float_register = (is_fp), .ssa_name = (ssa_name)})

struct v_register {
    struct ssa_name ssa_name;
    bool is_float_register;
    size_t register_bit_size;
};
