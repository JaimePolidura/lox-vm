#pragma once

#include "shared.h"
#include "vm/vm.h"
#include "types/function_object.h"
#include "memory/string_pool.h"
#include "memory/gc/gc_algorithm.h"

#define HEAP_GROW_AFTER_GC_FACTOR 2

struct gc_mark_sweep_thread_info {
    struct gc_thread_info gc_thread_info;

    size_t next_gc;

    //Linked list of heap allocated objects
    struct object * heap;
};

struct gc_mark_sweep {
    struct gc gc;

    int gray_count;
    int gray_capacity;
    struct object ** gray_stack;

    volatile int number_threads_ack_start_gc_signal;
    pthread_cond_t await_ack_start_gc_signal_cond;
    struct mutex await_ack_start_gc_signal_mutex;

    pthread_cond_t await_gc_cond;
    struct mutex await_gc_cond_mutex;
};
