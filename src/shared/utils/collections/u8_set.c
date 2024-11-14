#include "u8_set.h"

#define VALUE_TO_SLOT_INDEX(value) ((value) & 0x03)
#define VALUE_TO_SLOT_BIT_INDEX(value) ((value) >> 2)

struct u8_set create_u8_set(int n_elements, ...) {
    struct u8_set to_return;
    init_u8_set(&to_return);
    int elements[n_elements];
    VARARGS_TO_ARRAY(int, elements, n_elements);
    for(int i = 0; i < n_elements; i++){
        add_u8_set(&to_return, elements[i]);
    }

    return to_return;
}

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

void remove_u8_set(struct u8_set * set, uint8_t value_to_remove) {
    uint8_t slot_bit_index = VALUE_TO_SLOT_BIT_INDEX(value_to_remove);
    uint8_t slot_index = VALUE_TO_SLOT_INDEX(value_to_remove);
    uint64_t substraction_result = (((set->slot_bit_maps[slot_index] >> slot_bit_index) & 0x01) - 0x01);
    uint64_t new_slot_bit_map = (~substraction_result & 0x01) << slot_bit_index;
    set->slot_bit_maps[slot_index] ^= new_slot_bit_map;
}

void union_u8_set(struct u8_set * a, struct u8_set b) {
    a->slot_bit_maps[0] |= b.slot_bit_maps[0];
    a->slot_bit_maps[1] |= b.slot_bit_maps[1];
    a->slot_bit_maps[2] |= b.slot_bit_maps[2];
    a->slot_bit_maps[3] |= b.slot_bit_maps[3];
}

void difference_u8_set(struct u8_set * a, struct u8_set b) {
    a->slot_bit_maps[0] &= ~b.slot_bit_maps[0];
    a->slot_bit_maps[1] &= ~b.slot_bit_maps[1];
    a->slot_bit_maps[2] &= ~b.slot_bit_maps[2];
    a->slot_bit_maps[3] &= ~b.slot_bit_maps[3];
}

void intersection_u8_set(struct u8_set * a, struct u8_set b) {
    a->slot_bit_maps[0] &= b.slot_bit_maps[0];
    a->slot_bit_maps[1] &= b.slot_bit_maps[1];
    a->slot_bit_maps[2] &= b.slot_bit_maps[2];
    a->slot_bit_maps[3] &= b.slot_bit_maps[3];
}

uint8_t size_u8_set(struct u8_set set) {
    uint8_t a = __builtin_popcountll(set.slot_bit_maps[0]);
    uint8_t b = __builtin_popcountll(set.slot_bit_maps[1]);
    uint8_t c = __builtin_popcountll(set.slot_bit_maps[2]);
    uint8_t d = __builtin_popcountll(set.slot_bit_maps[3]);
    return a + b + c + d;
}