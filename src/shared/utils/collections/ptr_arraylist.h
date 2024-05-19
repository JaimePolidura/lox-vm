#pragma once

#include "shared/utils/utils.h"
#include "shared.h"

struct ptr_arraylist {
    void ** values;
    int capacity;
    int in_use;
};

void init_ptr_arraylist(struct ptr_arraylist * array);
void free_ptr_arraylist(struct ptr_arraylist * array);

//Returns the index where it has been added
uint16_t append_ptr_arraylist(struct ptr_arraylist * array, void * value);
