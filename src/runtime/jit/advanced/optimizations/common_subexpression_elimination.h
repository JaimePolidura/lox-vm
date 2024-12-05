#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

void perform_common_subexpression_elimination(struct ssa_ir *ssa_ir);
