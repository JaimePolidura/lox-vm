#include "mark_bitmap.h"

static inline uint8_t * get_slot_ptr(struct mark_bitmap *, void * addresss);
static inline uint8_t get_index_in_slot(void * address);

struct mark_bitmap * alloc_mark_bitmap(int n_addresses, void * start_address) {
    struct mark_bitmap * bitmap = malloc(sizeof(struct mark_bitmap));
    init_mark_bitmap(bitmap, n_addresses, start_address);
    return bitmap;
}

void init_mark_bitmap(struct mark_bitmap * mark_bitmap, int n_addresses, void * start_address) {
    size_t size_bitmap = round_up_8(n_addresses / 8);
    uint8_t * ptr = malloc(size_bitmap);

    mark_bitmap->start_address = start_address;
    mark_bitmap->end = ((uint8_t *) ptr) + size_bitmap;
    mark_bitmap->start = ptr;
    memset(ptr, 0, size_bitmap);
}

void free_mark_bitmap(struct mark_bitmap * mark_bitmap) {
    free(mark_bitmap->start);
}

void set_unmarked_bitmap(struct mark_bitmap * mark_bitmap, void * address) {
    uint8_t index_in_slot = get_index_in_slot(address);
    uint8_t * slot = get_slot_ptr(mark_bitmap, address);
    *slot &= ~(0x01 << index_in_slot);
}

void set_marked_bitmap(struct mark_bitmap * mark_bitmap, void * address) {
    uint8_t index_in_slot = get_index_in_slot(address);
    uint8_t * slot = get_slot_ptr(mark_bitmap, address);
    *slot |= (0x01 << index_in_slot);
}

bool is_marked_bitmap(struct mark_bitmap * mark_bitmap, void * address) {
    uint8_t slot_value = * get_slot_ptr(mark_bitmap, address);
    uint8_t index_in_slot = get_index_in_slot(address);
    return (slot_value & (0x01 << index_in_slot)) != 0;
}

void reset_mark_bitmap(struct mark_bitmap * mark_bitmap) {
    uint64_t * current = (uint64_t *) mark_bitmap->start;
    uint64_t * end = (uint64_t *) mark_bitmap->end;

    while (current < end) {
        *current++ = 0;
    }
}

bool for_each_marked_bitmap(struct mark_bitmap * bitmap, void * extra, mark_bitmap_consumer_t consumer) {
    bool continue_iterating = true;

    for (
            uint64_t * actual_slot = (uint64_t *) bitmap->start;
            actual_slot < (uint64_t *) bitmap->end && continue_iterating;
            actual_slot++
    ) {
        if (actual_slot != NULL && *actual_slot != 0) {
            uint64_t slot_value = *actual_slot;
            uint64_t slot_address = (uint64_t) ((uint64_t *) bitmap->start_address + (actual_slot - (uint64_t *) bitmap->start));

            for (int i = 0; i < sizeof(uint64_t) && continue_iterating; i++) {
                if (((slot_value >> i) & 0x1) != 0) {
                    uint64_t current_address = slot_address + i;
                    continue_iterating = consumer((void *) current_address, extra);
                }
            }
        }
    }

    return continue_iterating;
}

static inline uint8_t get_index_in_slot(void * address) {
    return ((uint64_t) address & 0x38) >> 3;
}

static inline uint8_t * get_slot_ptr(struct mark_bitmap * this, void * address) {
    uint64_t slot_index_address = ((uint64_t) address) >> 6;
    uint64_t slot_index_start_address = ((uint64_t) this->start_address) >> 6;

    return this->start + (slot_index_address - slot_index_start_address);
}
