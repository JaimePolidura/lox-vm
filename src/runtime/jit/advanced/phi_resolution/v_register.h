#pragma once

#include "shared.h"

#define TO_NUMBER_V_REGISTER(v_reg) ((v_reg).number << 32 | (uint8_t) (v_reg).is_float_register)
#define FROM_NUMBER_V_REGISTER(v_reg_number) ((struct v_register) { \
    .is_float_register = (bool) ((v_reg_number) & 0x01), \
    .number = (v_reg_number) >> 32, \
    .register_bit_size = 64, \
})

struct v_register {
    uint16_t number;
    bool is_float_register;
    size_t register_bit_size;
};
