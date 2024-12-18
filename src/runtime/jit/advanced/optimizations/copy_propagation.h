#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Replaces redundant & unused copies &. Example
//a1 = a0; b1 = a1 + 2 -> b1 = a0 + 2;
//a1 = a0 + 1; b1 = a1 + 2 -> b1 = (a0 + 1) + 2
void perform_copy_propagation(struct ssa_ir *);