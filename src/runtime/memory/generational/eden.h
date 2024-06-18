#pragma once

#include "shared/types/types.h"
#include "shared/config/config.h"
#include "shared.h"

struct eden {
    lox_value_t * start;
    lox_value_t * end;
    lox_value_t * current;

    int block_size_kb;
};

struct eden_thread {
    lox_value_t * start_block;
    lox_value_t * end_block;
    lox_value_t * current_block;
};

struct eden * alloc_eden(struct config config);
struct eden_thread * alloc_eden_thread();

lox_value_t * try_claim_block(struct eden *, int n_blocks);