#include "gc.h"

//Implemented by gc algorithm
extern void add_object_to_heap_gc_alg(struct gc_thread_info *, struct object *, size_t);
extern void check_gc_on_safe_point_alg();
extern struct gc_thread_info * alloc_gc_thread_info_alg();
extern struct gc * alloc_gc_alg();
extern struct gc_result start_gc_alg();
extern void on_gc_finished_vm(struct gc_result result);

struct gc_thread_info * alloc_gc_thread_info() {
    return alloc_gc_thread_info_alg();
}

struct gc * alloc_gc() {
    return alloc_gc_alg();
}

void init_gc_result(struct gc_result * gc_result) {
    gc_result->bytes_allocated_before_gc = 0;
    gc_result->bytes_allocated_after_gc = 0;
}

void add_object_to_heap(struct gc_thread_info * gc_thread_info, struct object * object, size_t size) {
    gc_thread_info->bytes_allocated += size;
    add_object_to_heap_gc_alg(gc_thread_info, object, size);
}

struct gc_result try_start_gc(struct gc_thread_info * gc_thread_info) {
    struct gc * gc_global_info = gc_thread_info->gc_global_info;
    struct gc_result gc_result;
    init_gc_result(&gc_result);
    gc_state_t expected = GC_NONE;

    if(atomic_compare_exchange_strong(&gc_global_info->state, &expected, GC_WAITING)) {
        gc_global_info->state = GC_WAITING;
        gc_result = start_gc_alg();
        gc_global_info->state = GC_NONE;

        on_gc_finished_vm(gc_result);
    } else { //Someone else has started a gc
        check_gc_on_safe_point_alg();
    }

    return gc_result;
}

int sizeof_heap_allocated_lox_object(struct object * object) {
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

void free_heap_allocated_lox_object(struct object * object) {
    switch (object->type) {
        case OBJ_STRING: {
            free(((struct string_object *) object)->chars);
            break;
        };
        case OBJ_STRUCT_INSTANCE: {
            struct struct_instance_object * struct_instance = (struct struct_instance_object *) object;
            free_hash_table(&struct_instance->fields);
            break;
        };
    }

    free(object);
}