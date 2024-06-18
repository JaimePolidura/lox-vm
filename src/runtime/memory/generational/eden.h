#pragma once

#include "shared/types/types.h"
#include "shared/config/config.h"
#include "shared.h"

struct eden {
    uint8_t * start;
    uint8_t * end;
    uint8_t * current;

    uint64_t size_blocks_in_bytes;
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