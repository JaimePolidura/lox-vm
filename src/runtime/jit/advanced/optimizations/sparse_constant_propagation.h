#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared.h"

//Implementation of "sparse simple constant propagation" algorithm
void perform_sparse_constant_propagation(struct ssa_creation_result);