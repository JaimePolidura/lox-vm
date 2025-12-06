#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared/types/types.h"
#include "shared/utils/utils.h"
#include "shared.h"

struct u8_arraylist {
    struct lox_allocator * allocator;
    uint8_t * values;
    int capacity;
    int in_use;
};

void init_u8_arraylist(struct u8_arraylist *, struct lox_allocator *);
void free_u8_arraylist(struct u8_arraylist *);

//Returns the index where it has been added
uint16_t append_u8_arraylist(struct u8_arraylist *, uint8_t value);
