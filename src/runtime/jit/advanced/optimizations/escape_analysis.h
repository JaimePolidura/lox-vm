#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Performs escape analysis on structs & arrays (instance) initialized in the ssa_ir. If an instance does/doesn't escape
//all the ssa node uses of that instance, the field escapes is set to true/false.
//Run after type_propagation and before box insertion. Uses type information.
//Escape analysis is useful:
// Allocate instances in the stack if possible
// Avoid inserting boxing/unboxing when getting/setting instsance fields

//An instance escapes if:
// Is passed as a function argument
// Is set to a global variable
// Is returned
// It is set to instance field/element of an instance that already escapes
void perform_escape_analysis(struct ssa_ir *ssa_ir);
