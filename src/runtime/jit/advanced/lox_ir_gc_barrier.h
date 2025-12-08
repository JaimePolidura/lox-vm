#pragma once

#include "lox_ir_type.h"

struct lox_ir_gc_write_barrier {
    bool requires_write_gc_barrier;
    lox_ir_type_t object_src_type;
};
