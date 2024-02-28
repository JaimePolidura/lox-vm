#pragma once

#include "shared.h"
#include "types/types.h"

struct lox_array_list {
    lox_value_t * values;
    int capacity;
    int in_use;
};

void init_lox_array(struct lox_array_list * array);
void write_lox_array(struct lox_array_list * array, lox_value_t value);
void flip_lox_array(struct lox_array_list * array);
void free_lox_array(struct lox_array_list * array);