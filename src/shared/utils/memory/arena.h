#pragma once

#include "shared/utils/utils.h"
#include "shared.h"
#include "lox_allocator.h"

#define ARENA_CONTENT_BLOCK_SIZE (4096 - 16)

struct arena_block {
    struct arena_block * prev;
    int next_index; //Index to next space available
    uint8_t bytes[ARENA_CONTENT_BLOCK_SIZE];
};

struct arena {
    struct arena_block * current;
};

void init_arena(struct arena *);
void free_arena(struct arena *);

void * malloc_arena(struct arena *, size_t to_alloc_size);
struct lox_allocator to_lox_allocator_arena(struct arena);