#include "mark_bitmap.h"

struct mark_bitmap * alloc_mark_bitmap(int n_addresses, uint64_t start_address) {
    struct mark_bitmap * bitmap = malloc(sizeof(struct mark_bitmap *));
    init_mark_bitmap(bitmap, n_addresses, start_address);
    return bitmap;
}

void init_mark_bitmap(struct mark_bitmap * mark_bitmap, int n_addresses, uint64_t start_address) {
    size_t size_bitmap = round_up_8(n_addresses / 8);
    uint8_t * ptr = malloc(size_bitmap);

    mark_bitmap->start_address = start_address;
    mark_bitmap->end = ptr + size_bitmap;
    mark_bitmap->start = ptr;
}

void set_marked_bitmap(struct mark_bitmap * mark_bitmap, uintptr_t address) {
    uint8_t index_in_slot = address >> 61;
    mark_bitmap->start[address - mark_bitmap->start_address] |= index_in_slot << 0x01;
}

bool is_marked_bitmap(struct mark_bitmap * mark_bitmap, uintptr_t address) {
    uint8_t slot = mark_bitmap->start[address - mark_bitmap->start_address];
    uint8_t index_in_slot = address >> 61;

    return (slot & (0x01 << index_in_slot)) != 0;
}

void reset_mark_bitmap(struct mark_bitmap * mark_bitmap) {
    uint64_t * current = (uint64_t *) mark_bitmap->start;
    uint64_t * end = (uint64_t *) mark_bitmap->start;

    while(current < end){
        *current++ = 0;
    }
}
