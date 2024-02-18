#include "gc_global_info.h"

//Implemented by gc algorithm
extern void start_gc_alg();
extern void setup_gc_alg();

//Implemented by vm. It will block the caller
extern void signal_threads_start_gc();

static void try_start_gc(struct gc_thread_info * gc_thread_info);

void init_gc_global_info(struct gc_global_info * gc) {
    gc->state = GC_NONE;
    setup_gc_alg();
}

void init_gc_thread_info(struct gc_thread_info * gc_per_thread) {
    gc_per_thread->heap = NULL;
    gc_per_thread->bytes_allocated = 0;
    gc_per_thread->next_gc = 1024 * 1024;
    gc_per_thread->gc_global_info = NULL;
}

void add_object_to_heap(struct gc_thread_info * gc_thread_info, struct object * object, size_t size) {
    object->next = gc_thread_info->heap;
    gc_thread_info->heap = object;

    gc_thread_info->bytes_allocated += size;

    if (gc_thread_info->bytes_allocated >= gc_thread_info->next_gc) {
        size_t before_gc_heap_size = gc_thread_info->bytes_allocated;
        try_start_gc(gc_thread_info);
        size_t after_gc_heap_size = gc_thread_info->bytes_allocated;

        gc_thread_info->next_gc = gc_thread_info->bytes_allocated * HEAP_GROW_AFTER_GC_FACTOR;

        printf("Collected %zu bytes (from %zu to %zu) next at %zu\n", before_gc_heap_size - after_gc_heap_size,
               before_gc_heap_size, after_gc_heap_size, gc_thread_info->next_gc);
    }
}

static void try_start_gc(struct gc_thread_info * gc_thread_info) {
    struct gc_global_info * gc_global_info = gc_thread_info->gc_global_info;
    gc_state_t expected = GC_NONE;

    if(atomic_compare_exchange_strong(&gc_global_info->state, &expected, GC_WAITING)) {
        signal_threads_start_gc();

        gc_global_info->state = GC_IN_PROGRESS;
        start_gc_alg();
        gc_global_info->state = GC_NONE;
    }
}