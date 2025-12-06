#pragma once

#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Performs escape analysis on structs & arrays (instance) initialized in the lox_ir. If an instance does/doesn't escape
//all the jit control uses of that instance, the field escapes is set to true/false.
//Run after type_propagation and before cast insertion. Uses type information.
//Escape analysis is useful:
// Allocate instances in the stack if possible
// Avoid inserting cast when getting/setting instsance fields

//An instance escapes if:
// Is passed as a function argument
// Is set to a global variable
// Is returned
// It is set to instance field/element of an instance that already escapes
void perform_escape_analysis(struct lox_ir *lox_ir);
