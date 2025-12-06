#pragma once

#include "runtime/jit/advanced/optimizations/type_propagation.h"
#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/collections/stack_list.h"
#include "shared/utils/memory/arena.h"
#include "shared.h"

//Introduces native types and inserts cast nodes in the IR when necessary
//Uses type information. Run after type_propagation
//Might introduce redundant csat expressions. Run before common_subexpression_elimination & loop invariant code motion
void perform_cast_insertion(struct lox_ir *lox_ir);
