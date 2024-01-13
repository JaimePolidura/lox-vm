#include "memory.h"

#include <stdlib.h>

extern void start_gc();

void * reallocate(void* pointer, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

//    start_gc();

    return realloc(pointer, new_size);
}
