#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Optmizes math operations into cheaper ones. Operations that are being optimized:
// Check for odd/even numbers:
//  number % 2 -> number & 0x01 == 0
//  number % k -> Where k is power of 2: number & (k - 1)
//Additions. Where number is not floating point:
//  k * 2 -> k << 1;
//  k * 5 -> k << 2 + number;
//Divisions:
//  number / k. Where k is power of 2 and number is not floating point: -> number >> k
void perform_strength_reduction(struct ssa_ir *ssa_ir);