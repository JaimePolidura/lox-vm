#pragma once

#include "runtime/threads/vm_thread.h"

#include "shared.h"
#include "shared/types/types.h"
#include "shared/types/string_object.h"
#include "shared/types/struct_instance_object.h"
#include "shared/types/struct_definition_object.h"
#include "shared/types/array_object.h"

//This serves as a wrapper around the specific tc algorithm

//Generic structure maintained by every thread (represented by struct vm_thread).
//The gc algorithm will implement its own per-thread structure.
struct gc_thread_info {
    size_t bytes_allocated;

    struct gc * gc_global_info;
};

struct gc_result {
    long bytes_allocated_before_gc;
    long bytes_allocated_after_gc;
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

struct gc_thread_info * alloc_gc_thread_info();
struct gc * alloc_gc();
void init_gc_result(struct gc_result * gc_result);

void add_object_to_heap(struct object * object);

int sizeof_heap_allocated_lox_object(struct object * object);
void free_heap_allocated_lox_object(struct object * object);

//Starts gc. Only used to manually force gc
//It may fail if another thread tries to start a gc concurrently
struct gc_result try_start_gc(struct gc_thread_info * gc_thread_info);