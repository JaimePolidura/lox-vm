#pragma once

#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Removes repeated data flow graphs in the lox_ir. This optimization takes account associative/commutative expressions
//Safe to run at any point in the compiling process.
void perform_common_subexpression_elimination(struct lox_ir *lox_ir);
