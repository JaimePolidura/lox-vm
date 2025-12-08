#pragma once

struct lox_ir_gc_write_barrier {
    bool requires_write_gc_barrier;
    bool requires_lox_any_type_check;
    bool requires_native_to_lox_pointer_cast;
};

struct lox_ir_gc_read_barrier {
    bool requires_read_gc_barrier;
};