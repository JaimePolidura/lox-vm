#pragma once

#include "runtime/jit/x64/operands.h"

typedef int(* single_operation_t)(struct jit_compiler *, struct lox_ir_ll_operand);

struct single_operation {
    single_operation_t imm_operation;
    single_operation_t reg_operation;
};