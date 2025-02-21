#pragma once

#include "runtime/jit/advanced/optimizations/type_propagation.h"
#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/collections/stack_list.h"
#include "shared/utils/memory/arena.h"
#include "shared.h"

//Inserts box/unbox nodes in the lox_ir
//Uses type information. Run after type_propagation
//Might introduce redundant unbox/box expressions. Run before common_subexpression_elimination & loop invariant code motion
void perform_unboxing_insertion(struct lox_ir *);
