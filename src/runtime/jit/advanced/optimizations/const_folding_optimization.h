#pragma once

#include "shared.h"
#include "runtime/jit/advanced/ssa_block.h"

void perform_const_folding_optimization(struct ssa_block * start_block);