#pragma once

#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Removes unncessary gc barriers.
//Run after cast_insertion.h
void perform_gc_barrier_elimination(struct lox_ir *);