#pragma once

#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Replaces redundant & unused copies &. Example
//a1 = a0; b1 = a1 + 2 -> b1 = a0 + 2;
//a1 = a0 + 1; b1 = a1 + 2 -> b1 = (a0 + 1) + 2
//Safe to run at any point in the compiling process. It is recommended to run at the last optimization phase, to simplify
//jit names, to generate machine code
//This can also run with v registers
void perform_copy_propagation(struct lox_ir *);