#pragma once

#include "../shared.h"
#include "types.h"

struct lox_array {
    lox_value_t * values;
    int capacity;
    int in_use;
};

void alloc_lox_array(struct lox_array * array);
void write_lox_array(struct lox_array * array, lox_value_t value);
void free_lox_array(struct lox_array * array);