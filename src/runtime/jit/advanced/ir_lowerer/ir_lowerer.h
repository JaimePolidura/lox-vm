#pragma once

#include "runtime/jit/advanced/ir_lowerer/ir_lowerer_control.h"
#include "runtime/jit/advanced/ir_lowerer/lllil.h"
#include "runtime/jit/advanced/lox_ir_ll_operand.h"
#include "runtime/jit/advanced/lox_ir.h"

struct lox_level_lox_ir_node * lower_lox_ir(struct lox_ir *);