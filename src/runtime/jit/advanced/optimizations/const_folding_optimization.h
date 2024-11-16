#pragma once

#include "shared.h"
#include "runtime/jit/advanced/ssa_block.h"

//Implementation of "sparse simple constant propagation" algorithm
void perform_const_folding_optimization(struct ssa_block * start_block);