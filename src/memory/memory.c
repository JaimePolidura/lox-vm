#include "memory.h"

#include <stdlib.h>

void * reallocate(void* pointer, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    return realloc(pointer, new_size);
}

void * reallocate_array(void* ptr, int new_size) {
    if(new_size == 0) {
        free(ptr);
        return NULL;
    }

    void * result = realloc(ptr, new_size);
    if (result == NULL) {
        perror("Out of memory");
        exit(1);
    }

    return result;
}
