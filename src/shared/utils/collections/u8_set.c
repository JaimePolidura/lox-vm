#include "u8_set.h"

#define VALUE_TO_SLOT_INDEX(value) ((value) & 0x03)
#define VALUE_TO_SLOT_BIT_INDEX(value) ((value) >> 2)

void init_u8_set(struct u8_set * set) {
    memset(set, 0, sizeof(struct u8_set));
}

void add_u8_set(struct u8_set * set, uint8_t value_to_add) {
    uint8_t slot_bit_index = VALUE_TO_SLOT_BIT_INDEX(value_to_add);
    uint8_t slot_index = VALUE_TO_SLOT_INDEX(value_to_add);
    set->slot_bit_maps[slot_index] |= (uint64_t) 0x01ULL << slot_bit_index;
}

bool contains_u8_set(struct u8_set * set, uint8_t value_to_check) {
    uint8_t slot_bit_index = VALUE_TO_SLOT_BIT_INDEX(value_to_check);
    uint8_t slot_index = VALUE_TO_SLOT_INDEX(value_to_check);
    uint64_t slot_bit_map_value = set->slot_bit_maps[slot_index];
    return (slot_bit_map_value & (uint64_t) (0x01ULL << slot_bit_index)) != 0x00;
}

uint8_t size_u8_set(struct u8_set set) {
    uint8_t a = __builtin_popcountll(set.slot_bit_maps[0]);
    uint8_t b = __builtin_popcountll(set.slot_bit_maps[1]);
    uint8_t c = __builtin_popcountll(set.slot_bit_maps[2]);
    uint8_t d = __builtin_popcountll(set.slot_bit_maps[3]);
    return a + b + c + d;
}