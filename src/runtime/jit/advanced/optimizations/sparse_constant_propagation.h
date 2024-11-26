#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Implementation of "sparse constant propagation" algorithm
void perform_sparse_constant_propagation(struct ssa_ir *ssa_ir);