#pragma once

#include "utils/collections/lox/lox_array_list.h"
#include "types.h"

struct array_object {
    struct object object;
    lox_value_t * values;
    int n_elements;
};

struct array_object * alloc_array_object();
void empty_initialization(struct array_object *, int);