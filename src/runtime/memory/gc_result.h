#pragma once

struct gc_result {
    long bytes_allocated_before_gc;
    long bytes_allocated_after_gc;
};

void init_gc_result(struct gc_result * gc_result);