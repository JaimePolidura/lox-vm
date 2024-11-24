#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared/types/types.h"
#include "shared/utils/utils.h"
#include "shared.h"


struct lox_arraylist {
    struct lox_allocator allocator;
    lox_value_t * values;
    int capacity;
    int in_use;
};

void init_lox_arraylist(struct lox_arraylist * array, struct lox_allocator allocator);
void init_lox_arraylist_with_size(struct lox_arraylist * array, int size, struct lox_allocator allocator);

void append_lox_arraylist(struct lox_arraylist * array, lox_value_t value);
void flip_lox_arraylist(struct lox_arraylist * array);
void free_lox_arraylist(struct lox_arraylist * array);