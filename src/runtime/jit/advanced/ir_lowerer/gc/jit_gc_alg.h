#pragma once

#include "runtime/jit/advanced/ir_lowerer/lllil.h"

//Every gc algorithm can implement these methods, so that the JIT and the GC is integrated

//Emits the low level lox ir nodes to represent the code of the write barrier. If the gc algorithm has a gc write barrier
//this will need to be defined.
void emit_write_barrier_ll_lox_ir_jit_gc_alg(struct lllil_control*,struct lox_ir_ll_operand,struct lox_ir_ll_operand);