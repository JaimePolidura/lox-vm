#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Removes repeated data flow graphs in the ssa_ir. This optimization takes account associative/commutative expressions
//Safe to run at any point in the compiling process.
void perform_common_subexpression_elimination(struct ssa_ir *ssa_ir);
