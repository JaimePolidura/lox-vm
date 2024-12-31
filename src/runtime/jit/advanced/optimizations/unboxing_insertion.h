#pragma once

#include "runtime/jit/advanced/optimizations/type_propagation.h"
#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Inserts box/unbox nodes in the ssa_ir
//Uses type information. Run after type_propagation
//Might introduce redundant expressions. Run before common_subexpression_elimination
void perform_unboxing_insertion(struct ssa_ir *);
