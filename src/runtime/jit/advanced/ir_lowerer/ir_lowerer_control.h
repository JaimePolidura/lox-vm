#pragma once

#include "runtime/jit/advanced/ir_lowerer/ir_lowerer_data.h"
#include "runtime/jit/advanced/ir_lowerer/utils.h"
#include "runtime/jit/advanced/ir_lowerer/lllil.h"
#include "runtime/jit/advanced/lox_ir_ll_operand.h"
#include "runtime/jit/advanced/lox_ir.h"

#include "shared/types/struct_instance_object.h"
#include "shared/types/array_object.h"

void lower_lox_ir_control(struct lllil_control *);
