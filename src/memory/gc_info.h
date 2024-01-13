#pragma once

#include "shared.h"
#include "types/types.h"

#define HEAP_GROW_AFTER_GC_FACTOR 2

struct gc_info {
    struct object * heap; // Linked list of heap allocated objects

    int gray_count;
    int gray_capacity;
    struct object ** gray_stack;

    size_t bytes_allocated;
    size_t next_gc;
};

void init_gc_info(struct gc_info * gc_info);
void add_value_gc_info(struct gc_info * gc_info, struct object * object);
void add_object_to_heap(struct gc_info * gc_info, struct object * object, size_t size);