#include "gc.h"

//Implemented by gc algorithm
extern void add_object_to_heap_gc_alg(struct gc_thread_info *, struct object *, size_t);
extern void start_gc_alg();
extern void init_gc_alg();

void init_gc_global_info(struct gc * gc) {
    gc->state = GC_NONE;
    init_gc_alg();
}

void init_gc_thread_info(struct gc_thread_info * gc_per_thread) {
    gc_per_thread->bytes_allocated = 0;
    gc_per_thread->gc_global_info = NULL;
}

void add_object_to_heap(struct gc_thread_info * gc_thread_info, struct object * object, size_t size) {
    gc_thread_info->bytes_allocated += size;
    add_object_to_heap_gc_alg(gc_thread_info, object, size);
}

void try_start_gc(struct gc_thread_info * gc_thread_info) {
    struct gc * gc_global_info = gc_thread_info->gc_global_info;
    gc_state_t expected = GC_NONE;

    if(atomic_compare_exchange_strong(&gc_global_info->state, &expected, GC_WAITING)) {
        gc_global_info->state = GC_WAITING;
        start_gc_alg();
        gc_global_info->state = GC_NONE;
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