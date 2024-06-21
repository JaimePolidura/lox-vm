#pragma once

#include "shared.h"

#include "runtime/memory/generational/mark_bitmap.h"
#include "shared/config/config.h"

struct card_table {
    uint64_t * start_address_memory_space;
    int n_addresses_per_card_table;

    struct mark_bitmap ** cards;
};

struct card_table * alloc_card_table(struct config,
        uint64_t * memory_space_start_address,
        uint64_t * memory_space_end_address);

void mark_dirty_card_table(struct card_table *, uint64_t * address);

bool is_dirty_card_table(struct card_table *, uint64_t * address);