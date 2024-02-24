#include "gc.h"

//Implemented by gc algorithm
extern void start_gc_alg();
extern void init_gc_alg();

//Implemented by vm. It will block the caller
extern void signal_threads_start_gc_alg();
extern void signal_threads_gc_finished_alg();

void init_gc_global_info(struct gc * gc) {
    gc->state = GC_NONE;
    init_gc_alg();
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

void try_start_gc(struct gc_thread_info * gc_thread_info) {
    struct gc * gc_global_info = gc_thread_info->gc_global_info;
    gc_state_t expected = GC_NONE;

    if(atomic_compare_exchange_strong(&gc_global_info->state, &expected, GC_WAITING)) {
        signal_threads_start_gc_alg();

        gc_global_info->state = GC_IN_PROGRESS;
        start_gc_alg();
        gc_global_info->state = GC_NONE;

        signal_threads_gc_finished_alg();
    }
}

int sizeof_heap_allocated_lox(struct object * object) {
    switch (object->type) {
        case OBJ_STRING: return ((struct string_object *) object)->length + 1;
        case OBJ_STRUCT_INSTANCE: {
            struct struct_instance_object * instance = (struct struct_instance_object *) object;
            struct struct_definition_object * definition = instance->definition;

            return sizeof(struct struct_instance_object) + definition->n_fields * sizeof(lox_value_t);
        }
        default: return 0;
    }
}