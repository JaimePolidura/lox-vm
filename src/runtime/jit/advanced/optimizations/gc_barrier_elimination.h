#pragma once

#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Removes unncessary gc barriers. A write barrier is considered usless when:
// - The value to be written is not an object type
// - The holder of the object to be written (struct or array) doesn't escape the method
//Run after escape_analysis.h
void perform_gc_barrier_elimination(struct lox_ir *);