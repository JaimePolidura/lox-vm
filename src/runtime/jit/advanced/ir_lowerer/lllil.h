#pragma once

#include "runtime/jit/advanced/lox_ir.h"

//Low level lox ir lowerer
struct lllil {
    struct lox_ir * lox_ir;
    struct arena_lox_allocator lllil_allocator;
};

struct lllil * alloc_low_level_lox_ir_lowerer(struct lox_ir *);
void free_low_level_lox_ir_lowerer(struct lllil *);