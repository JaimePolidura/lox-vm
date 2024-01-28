#include "gc.h"

extern void start_gc();
extern void setup_gc();

void init_gc_info(struct gc * gc_info) {
    gc_info->heap = NULL;
    gc_info->bytes_allocated = 0;
    gc_info->next_gc = 1024 * 1024;
    setup_gc();
}

void add_object_to_heap(struct gc * gc_info, struct object * object, size_t size) {
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