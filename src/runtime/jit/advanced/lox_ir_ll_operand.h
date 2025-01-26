#pragma once

#include "shared.h"
#include "runtime/jit/advanced/phi_resolution/v_register.h"

#define IMMEDIATE_TO_OPERAND(immediate) ((struct lox_ir_ll_operand) {LOX_IR_LL_OPERAND_IMMEDIATE, immediate})

typedef enum {
    LOX_IR_LL_OPERAND_REGISTER,
    LOX_IR_LL_OPERAND_IMMEDIATE,
    LOX_IR_LL_OPERAND_MEMORY_ADDRESS,
} low_ir_ll_operand_type_type;

struct lox_ir_ll_operand {
    low_ir_ll_operand_type_type type;

    union {
        struct v_register v_register;
        uint64_t immedate;
        struct {
            struct lox_ir_ll_operand * address;
            uint64_t offset;
        } memory_address;
    };
};