#pragma once

#include "runtime/jit/advanced/lox_ir.h"
#include "runtime/jit/advanced/register_allocation/live_range_analysis.h"

void perform_register_allocation(struct lox_ir *);