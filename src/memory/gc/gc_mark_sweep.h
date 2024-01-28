#pragma once

#include "shared.h"
#include "vm/vm.h"
#include "types/function.h"
#include "memory/string_pool.h"

struct gc_mark_sweep {
    struct gc gc;

    int gray_count;
    int gray_capacity;
    struct object ** gray_stack;
};

void start_gc();
void setup_gc();
