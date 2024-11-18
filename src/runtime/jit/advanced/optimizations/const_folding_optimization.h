#pragma once

#include "runtime/jit/advanced/creation/ssa_phi_inserter.h"
#include "runtime/jit/advanced/ssa_block.h"

#include "shared.h"

//Implementation of "sparse simple constant propagation" algorithm
void perform_const_folding_optimization(struct ssa_block *, struct phi_insertion_result);