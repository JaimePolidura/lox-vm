#pragma once

#include "shared.h"

struct u8_set {
    //In uin64 we can represent 64 elements: 64 * 4 = 256
    //In u8_set we can represent as much as 255 elements
    uint64_t slot_bit_maps[4];
};

void init_u8_set(struct u8_set *);
void add_u8_set(struct u8_set *, uint8_t value);
bool contains_u8_set(struct u8_set *, uint8_t value);
uint8_t size_u8_set(struct u8_set *);