#pragma once

#include "../shared.h"

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity << 1)
#define ALLOCATE(type, count) (type *) reallocate(NULL, 0, sizeof(type) * count)
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

void * reallocate(void* pointer, size_t old_size, size_t new_size);
