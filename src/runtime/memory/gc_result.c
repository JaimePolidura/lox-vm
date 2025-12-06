#include "gc_result.h"

void init_gc_result(struct gc_result * gc_result) {
    gc_result->bytes_allocated_before_gc = 0;
    gc_result->bytes_allocated_after_gc = 0;
}