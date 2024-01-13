#include "gc_info.h"

extern void start_gc();

void init_gc_info(struct gc_info * gc_info) {
    gc_info->gray_stack = NULL;
    gc_info->gray_capacity = 0;
    gc_info->gray_count = 0;
    gc_info->heap = NULL;
    gc_info->bytes_allocated = 0;
    gc_info->next_gc = 1024 * 1024;
}

void add_value_gc_info(struct gc_info * gc_info, struct object * value) {
    if (gc_info->gray_capacity < gc_info->gray_count + 1) {
        gc_info->gray_capacity = GROW_CAPACITY(gc_info->gray_capacity);
        gc_info->gray_stack = realloc(gc_info->gray_stack, sizeof(struct object*) * gc_info->gray_capacity);
    }

    gc_info->gray_stack[gc_info->gray_count++] = value;
}

void add_object_to_heap(struct gc_info * gc_info, struct object * object, size_t size) {
    object->next = gc_info->heap;
    gc_info->heap = object;

    gc_info->bytes_allocated += size;

    if(gc_info->bytes_allocated >= gc_info->next_gc){
        size_t before_gc_heap_size = gc_info->bytes_allocated;
        start_gc();
        size_t after_gc_heap_size = gc_info->bytes_allocated;

        gc_info->next_gc = gc_info->bytes_allocated * HEAP_GROW_AFTER_GC_FACTOR;
        
        printf("Collected %zu bytes (from %zu to %zu) next at %zu\n", before_gc_heap_size - after_gc_heap_size,
               before_gc_heap_size, after_gc_heap_size, gc_info->next_gc);
    }
}