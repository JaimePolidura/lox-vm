#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Moves loop invariant code (code that does produce the same output in every iteration) outside the loop.
//Safe to run at any point in the compiling process.
void perform_loop_invariant_code_motion(struct ssa_ir *ssa_ir);
