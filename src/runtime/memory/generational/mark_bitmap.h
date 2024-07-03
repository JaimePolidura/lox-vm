#pragma once

#include "shared.h"
#include "shared/utils/utils.h"

struct mark_bitmap {
    uint8_t * start_address;

    uint8_t * start;
    uint8_t * end;
};

typedef bool (*mark_bitmap_consumer_t)(void * address, void * extra);

struct mark_bitmap * alloc_mark_bitmap(int n_addresses, void * start_address);
void init_mark_bitmap(struct mark_bitmap *, int n_addresses, void * start_address);
void free_mark_bitmap(struct mark_bitmap *);

void set_marked_bitmap(struct mark_bitmap *, void * address);
void set_unmarked_bitmap(struct mark_bitmap *, void * address);
bool is_marked_bitmap(struct mark_bitmap *, void * address);
void reset_mark_bitmap(struct mark_bitmap *);
bool for_each_marked_bitmap(struct mark_bitmap *, void * extra, mark_bitmap_consumer_t consumer);