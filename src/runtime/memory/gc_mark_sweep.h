#pragma once

#include "shared.h"
#include "runtime/vm.h"
#include "shared/types/function_object.h"
#include "shared/string_pool.h"
#include "gc_algorithm.h"

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
