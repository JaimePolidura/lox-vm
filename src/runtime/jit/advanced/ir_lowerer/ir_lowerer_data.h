#pragma once

#include "runtime/jit/advanced/ir_lowerer/utils.h"
#include "runtime/jit/advanced/ir_lowerer/lllil.h"
#include "runtime/jit/advanced/lox_ir_ll_operand.h"
#include "runtime/jit/advanced/lox_ir.h"

struct lox_ir_ll_operand lower_lox_ir_data(
        struct lllil*,
        struct lox_ir_data_node *,
        lox_ir_type_t expected_type
);
