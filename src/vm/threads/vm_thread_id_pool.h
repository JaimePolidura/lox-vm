#pragma once

#include "shared.h"

#define MAX_REUSABLE_THREAD_IDS_POOL 64

struct thread_id_slot {
    uint64_t cache_line_pad[64];
    volatile bool taken;
};

struct thread_id_pool {
    struct thread_id_slot slots[MAX_REUSABLE_THREAD_IDS_POOL];
    lox_thread_id last;
};

void init_thread_id_pool(struct thread_id_pool *);

lox_thread_id acquire_thread_id_pool(struct thread_id_pool * pool);
void release_thread_id_pool(struct thread_id_pool * pool, lox_thread_id to_release);
