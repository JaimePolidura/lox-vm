#include "memory_space.h"

struct memory_space * alloc_memory_space(size_t size_in_bytes) {
    struct memory_space * memory_space = malloc(sizeof(struct memory_space));
    init_memory_space(memory_space, size_in_bytes);
    return memory_space;
}

void init_memory_space(struct memory_space * memory_space, size_t size_in_bytes) {
    memory_space->start = malloc(size_in_bytes);
    memory_space->current = memory_space->start;
    memory_space->end = memory_space->start + size_in_bytes;
    memory_space->size_in_bytes = size_in_bytes;
}

uint8_t * alloc_data_memory_space(struct memory_space * memory_space, size_t size_in_bytes) {
    if(memory_space->current + size_in_bytes > memory_space->end){
        return NULL;
    }

    uint8_t * ptr = memory_space->current;
    memory_space->current += size_in_bytes;
    return ptr;
}

void copy_data_at_memory_space(struct memory_space * memory_space, uint8_t * dst, uint8_t * src, size_t size_in_bytes) {
    if (memory_space->current + size_in_bytes > memory_space->end) {
        return;
    }

    for (int i = 0; i < size_in_bytes; i++) {
        *(dst++) = *(src++);
    }
}

uint8_t * copy_data_memory_space(struct memory_space * memory_space, uint8_t * src, size_t size_in_bytes) {
    if (memory_space->current + size_in_bytes > memory_space->end) {
        return NULL;
    }

    uint8_t * dst = memory_space->current;
    uint8_t * start_moved = memory_space->current;

    for (int i = 0; i < size_in_bytes; i++) {
        *(dst++) = *(src++);
    }

    memory_space->current = dst;

    return start_moved;
}

void reset_memory_space(struct memory_space * memory_space) {
    memory_space->current = memory_space->start;
}

bool belongs_to_memory_space(struct memory_space * memory_space, uintptr_t ptr) {
    return ((uintptr_t) memory_space->start) <= ptr && ((uintptr_t) memory_space->end) > ptr;
}

size_t get_allocated_bytes_memory_space(struct memory_space * memory_space) {
    return memory_space->current - memory_space->start;
}