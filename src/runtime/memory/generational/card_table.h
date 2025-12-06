#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared/config/config.h"
#include "shared.h"

#include "runtime/memory/generational/mark_bitmap.h"

struct card_table {
    uint64_t * start_address_memory_space;
    int n_addresses_per_card_table;
    int n_cards;

    struct mark_bitmap ** cards;
};

struct card_table * alloc_card_table(struct config, uint64_t * memory_space_start_address, uint64_t * memory_space_end_address);
void init_card_table(struct card_table *, struct config config, uint64_t * memory_space_start_address, uint64_t * memory_space_end_address);

void mark_dirty_card_table(struct card_table *, uint64_t * address);
bool is_dirty_card_table(struct card_table *, uint64_t * address);
void for_each_card_table(struct card_table * table, void * extra, mark_bitmap_consumer_t);
void clear_card_table(struct card_table * table);