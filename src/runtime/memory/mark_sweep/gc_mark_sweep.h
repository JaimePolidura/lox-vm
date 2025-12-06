#pragma once

#include "shared.h"
#include "runtime/vm.h"
#include "shared/types/function_object.h"
#include "shared/string_pool.h"
#include "runtime/memory/gc_algorithm.h"
#include "runtime/memory/gc_result.h"
#include "runtime/memory/utils.h"

typedef enum {
    GC_NONE, //No gc is being performed
    GC_WAITING, //Waiting to all threads_race_conditions to stop
    GC_IN_PROGRESS, //Performing GC
} gc_mark_sweep_state_t;

//Global structure to hold profile_data about gc
struct mark_sweep_global_info {
    volatile gc_mark_sweep_state_t state;

    int gray_count;
    int gray_capacity;
    struct object ** gray_stack;

    volatile int number_threads_ack_start_gc_signal;
    pthread_cond_t await_ack_start_gc_signal_cond;
    struct mutex await_ack_start_gc_signal_mutex;
    pthread_cond_t await_gc_cond;
    struct mutex await_gc_cond_mutex;
};

//Per-thread gc info struct
struct mark_sweep_thread_info {
    size_t bytes_allocated;

    struct mark_sweep_global_info * mark_sweep;

    size_t next_gc;

    //Linked list of heap allocated objects maitained per each thread
    struct object * heap;
};