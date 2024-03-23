#pragma once

#include "shared.h"
#include "shared/types/types.h"
#include "shared/utils/utils.h"

struct u8_arraylist {
    uint8_t * values;
    int capacity;
    int in_use;
};

void init_u8_arraylist(struct u8_arraylist * array);
void free_u8_arraylist(struct u8_arraylist * array);

void append_u8_arraylist(struct u8_arraylist * array, uint8_t value);
