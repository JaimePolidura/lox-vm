#pragma once

#include "shared.h"

struct v_register {
    bool is_float_register;
    uint16_t number;
    size_t register_bit_size;
};
