#pragma once

#include "shared.h"
#include "types/types.h"

struct lox_arraylist {
    lox_value_t * values;
    int capacity;
    int in_use;
};

void init_lox_arraylist(struct lox_arraylist * array);
void init_lox_arraylist_with_size(struct lox_arraylist * array, int size);

void append_lox_arraylist(struct lox_arraylist * array, lox_value_t value);
void flip_lox_arraylist(struct lox_arraylist * array);
void free_lox_arraylist(struct lox_arraylist * array);