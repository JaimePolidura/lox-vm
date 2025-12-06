#pragma once

#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

void perform_range_check_elimination(struct lox_ir *lox_ir);
