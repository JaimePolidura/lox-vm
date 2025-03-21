#pragma once

#include "runtime/jit/x64/operands.h"

typedef int(* binary_operation_t)(struct jit_compiler *, struct operand, struct operand);

struct binary_operation {
    binary_operation_t imm_imm_operation;
    binary_operation_t reg_reg_operation;
    binary_operation_t reg_imm_operation;
};