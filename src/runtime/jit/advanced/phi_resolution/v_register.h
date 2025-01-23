#pragma once

#include "shared.h"

#define TO_NUMBER_V_REGISTER(v_reg) ((v_reg).reg_number << 32 | (uint8_t) (v_reg).is_float_register)
#define FROM_NUMBER_V_REGISTER(v_reg_number) ((struct v_register) { \
    .is_float_register = (bool) ((v_reg_number) & 0x01), \
    .reg_number = (v_reg_number) >> 32 \
})

struct v_register {
    uint16_t reg_number;
    bool is_float_register;
};
