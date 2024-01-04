#pragma once

#include "../shared.h"

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity << 2)
#define ALLOCATE(type, count) (type *) reallocate(NULL, 0, sizeof(type) * count)
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

void * reallocate(void* pointer, size_t old_size, size_t new_size);

void * reallocate_array(void* ptr, int new_size);