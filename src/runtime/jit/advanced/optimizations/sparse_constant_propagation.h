#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Performs constant folding & propagates its values. It also removes death branches.
//Implementation of "sparse constant propagation" algorithm
//Doesn't preserve types. Run before type propagation.
void perform_sparse_constant_propagation(struct ssa_ir *ssa_ir);