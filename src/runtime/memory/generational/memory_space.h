#pragma once

#include "shared/utils/memory/lox_allocator.h"
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
uint8_t * copy_data_memory_space(struct memory_space *, uint8_t * src, size_t size_in_bytes);
void copy_data_at_memory_space(struct memory_space *, uint8_t * dst, uint8_t * src, size_t size_in_bytes);
size_t get_allocated_bytes_memory_space(struct memory_space *);
void reset_memory_space(struct memory_space *);

bool belongs_to_memory_space(struct memory_space *, uintptr_t ptr);