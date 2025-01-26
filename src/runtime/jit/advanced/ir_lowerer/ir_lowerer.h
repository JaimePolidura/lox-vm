#pragma once

#include "runtime/jit/advanced/lox_ir.h"
#include "runtime/jit/advanced/lox_ir_ll_operand.h"

struct lox_level_lox_ir_node * lower_lox_ir(struct lox_ir *);