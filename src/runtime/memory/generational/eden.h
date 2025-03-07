#pragma once

#include "runtime/memory/generational/memory_space.h"
#include "runtime/memory/generational/mark_bitmap.h"
#include "runtime/memory/generational/card_table.h"

#include "shared/utils/memory/lox_allocator.h"
#include "shared/config/config.h"
#include "shared/types/types.h"
#include "shared.h"

struct eden {
    //Used in marking & updating references
    struct mark_bitmap * mark_bitmap;
    struct card_table card_table;

    struct memory_space memory_space;

    size_t size_blocks_in_bytes;
};

struct eden_thread {
    uint8_t * start_block;
    uint8_t * end_block;
    uint8_t * current_block;
};

struct eden * alloc_eden(struct config config);
struct eden_thread * alloc_eden_thread();

struct eden_block_allocation {
    uint8_t * start_block;
    uint8_t * end_block;
    bool success;
};

struct eden_block_allocation try_claim_eden_block(struct eden *, int n_blocks);
bool can_allocate_object_in_block_eden(struct eden_thread *, size_t size_bytes);
struct object * allocate_object_in_block_eden(struct eden_thread *, size_t size_bytes);
bool belongs_to_eden(struct eden *, uintptr_t ptr);
