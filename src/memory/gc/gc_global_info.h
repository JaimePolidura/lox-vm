#pragma once

#include "shared.h"
#include "types/types.h"

#define HEAP_GROW_AFTER_GC_FACTOR 2

struct gc_thread_info {
    struct object * heap; // Linked list of heap allocated objects

    size_t bytes_allocated;
    size_t next_gc;

    struct gc_global_info * gc_global_info;
};

typedef enum {
    GC_NONE, //No gc is being performed
    GC_WAITING, //Waiting to all threads to stop
    GC_IN_PROGRESS, //Performing GC
} gc_state_t;

struct gc_global_info {
    volatile gc_state_t state;
};

void init_gc_thread_info(struct gc_thread_info * gc_per_thread);
void init_gc_global_info(struct gc_global_info * gc);

void add_object_to_heap(struct gc_thread_info * gc_thread_info, struct object * object, size_t size);