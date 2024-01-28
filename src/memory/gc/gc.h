#pragma once

#include "shared.h"
#include "types/types.h"

#define HEAP_GROW_AFTER_GC_FACTOR 2

struct gc {
    struct object * heap; // Linked list of heap allocated objects

    size_t bytes_allocated;
    size_t next_gc;
};

void init_gc_info(struct gc * gc_info);
void add_object_to_heap(struct gc * gc_info, struct object * object, size_t size);