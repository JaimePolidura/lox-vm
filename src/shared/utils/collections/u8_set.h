#pragma once

#include "shared.h"

struct u8_set {
    //In uin64 we can represent 64 elements: 64 * 4 = 256
    //In u8_set we can represent as much as 255 elements
    uint64_t slot_bit_maps[4];
};

void init_u8_set(struct u8_set *);
void add_u8_set(struct u8_set *, uint8_t);
bool contains_u8_set(struct u8_set *, uint8_t);
void remove_u8_set(struct u8_set *, uint8_t);
void union_u8_set(struct u8_set *, struct u8_set);
//Example: a: {1, 2} b: {2, 3}, a - b = {1}
void difference_u8_set(struct u8_set * a, struct u8_set b);
void intersection_u8_set(struct u8_set *, struct u8_set);

uint8_t size_u8_set(struct u8_set);
