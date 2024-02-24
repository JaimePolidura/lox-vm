#pragma once

#include "shared.h"
#include "types/types.h"
#include "types/string_object.h"
#include "types/struct_instance_object.h"
#include "types/struct_definition_object.h"

#define HEAP_GROW_AFTER_GC_FACTOR 2

//Structure maintained by every thread, it wil hold data about heap allocated objects, bytes allocated etc
struct gc_thread_info {
    //Linked list of heap allocated objects
    struct object * heap;

    size_t bytes_allocated;
    size_t next_gc;

    struct gc * gc_global_info;
};

typedef enum {
    GC_NONE, //No gc is being performed
    GC_WAITING, //Waiting to all threads_race_conditions to stop
    GC_IN_PROGRESS, //Performing GC
} gc_state_t;

//Global structure to hold data about gc. This will be "inherited" by every gc algorithm
//(contained as a field of the specific gc algorithm)
struct gc {
    volatile gc_state_t state;
};

void init_gc_thread_info(struct gc_thread_info * gc_per_thread);
void init_gc_global_info(struct gc * gc);

void add_object_to_heap(struct gc_thread_info * gc_thread_info, struct object * object, size_t size);

int sizeof_heap_allocated_lox(struct object * object);

//Starts gc. Only used to manually force gc
void try_start_gc(struct gc_thread_info * gc_thread_info);