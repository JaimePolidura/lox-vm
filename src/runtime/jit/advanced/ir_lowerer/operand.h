#pragma once

#include "shared.h"
#include "runtime/jit/advanced/phi_resolution/v_register.h"

#define IMMEDIATE_TO_OPERAND(immediate) ((struct operand) {LOW_LEVEL_LOX_IR_OPERAND_IMMEDIATE, immediate})

typedef enum {
    LOW_LEVEL_LOX_IR_OPERAND_REGISTER,
    LOW_LEVEL_LOX_IR_OPERAND_IMMEDIATE,
    LOW_LEVEL_LOX_IR_OPERAND_MEMORY_ADDRESS,
} low_level_lox_ir_operand_type_type;

struct operand {
    low_level_lox_ir_operand_type_type type;

    union {
        struct v_register v_register;
        uint64_t immedate;
        struct {
            struct operand * address;
            uint64_t offset;
        } memory_address;
    };
};