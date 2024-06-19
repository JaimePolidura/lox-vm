#pragma once

#include "shared.h"

struct memory_space {
    uint8_t * start;
    uint8_t * end;
    uint8_t * current;

    uint64_t size_in_bytes;
};

struct memory_space * alloc_memory_space(size_t size_in_bytes);
void init_memory_space(struct memory_space *, size_t size_in_bytes);
uint8_t * alloc_data_memory_space(struct memory_space *, size_t size_in_bytes);
uint8_t * copy_data_memory_space(struct memory_space *, uint8_t * src, size_t size);

bool belongs_to_memory_space(struct memory_space *, uintptr_t ptr);