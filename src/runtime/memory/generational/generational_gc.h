#pragma once

#include "runtime/memory/generational/survivor.h"
#include "runtime/memory/generational/eden.h"
#include "runtime/memory/generational/old.h"
#include "runtime/memory/gc_algorithm.h"
#include "runtime/memory/gc_result.h"

#include "runtime/threads/vm_thread.h"
#include "runtime/vm.h"

typedef enum {
    GC_NONE, //No gc is being performed
    GC_WAITING, //Waiting to all threads_race_conditions to stop
    GC_IN_PROGRESS, //Performing GC
} gc_gen_state_t;

//Global struct. Maintained in vm.h
struct generational_gc {
    struct eden * eden;
    struct survivor * survivor;
    struct old * old;
    bool previous_major;

    volatile gc_gen_state_t state;

    volatile int number_threads_ack_start_gc_signal;
    pthread_cond_t await_ack_start_gc_signal_cond;
    struct mutex await_ack_start_gc_signal_mutex;
    pthread_cond_t await_gc_cond;
    struct mutex await_gc_cond_mutex;
};

//Per thread. Maintained in vm_thread.h
struct generational_thread_gc {
    struct eden_thread * eden;
};

bool belongs_to_young_generational_gc(struct generational_gc *, uintptr_t ptr);
struct card_table * get_card_table_generational_gc(struct generational_gc *, uintptr_t ptr);
void clear_mark_bitmaps_generational_gc(struct generational_gc *);
bool belongs_to_heap_generational_gc(struct generational_gc *, uintptr_t ptr);
void clear_card_tables_generational_gc(struct generational_gc *);
struct mark_bitmap * get_mark_bitmap_generational_gc(struct generational_gc *, uintptr_t ptr);
bool is_marked_generational_gc(struct generational_gc *, uintptr_t ptr);
size_t get_bytes_allocated_generational_gc(struct generational_gc *);