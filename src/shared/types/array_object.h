#pragma once

#include "shared/utils/collections/lox_arraylist.h"
#include "types.h"

struct array_object {
    struct object object;
    struct lox_arraylist values;
};

//Returns true if successful (not out of bounds)
bool set_element_array(struct array_object * array, int index, lox_value_t value);