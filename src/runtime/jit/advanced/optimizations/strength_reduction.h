#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Transforms math operations into cheaper ones.
//Uses type information. Run after type propagation optimization process.
//Operations that are being optimized to be cheaper:
// n % 2 -> n & 0x01 == 0
// m % n -> m & (n - 1) Where n is power of 2
// n * 2 -> n << 1; Where n is not f64 type
// n * 5 -> n << 2 + m; Where n is not f64 type
// m / n -> m >> k Where n is power of 2 and m is not f64
void perform_strength_reduction(struct ssa_ir *ssa_ir);