#pragma once

#include "shared.h"
#include "shared/utils/utils.h"

struct mark_bitmap {
    uint64_t * start_address;

    uint8_t * start;
    uint8_t * end;
};

struct mark_bitmap * alloc_mark_bitmap(int n_addresses, uint64_t * start_address);
void init_mark_bitmap(struct mark_bitmap *, int n_addresses, uint64_t * start_address);
void free_mark_bitmap(struct mark_bitmap *);

void set_marked_bitmap(struct mark_bitmap *, uint64_t * address);
void set_unmarked_bitmap(struct mark_bitmap *, uint64_t * address);
bool is_marked_bitmap(struct mark_bitmap *, uint64_t * address);
void reset_mark_bitmap(struct mark_bitmap *);