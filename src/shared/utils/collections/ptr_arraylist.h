#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared/utils/utils.h"
#include "shared.h"

struct ptr_arraylist {
    struct lox_allocator * allocator;
    void ** values;
    int capacity;
    int in_use;
};

struct ptr_arraylist * alloc_ptr_arraylist(struct lox_allocator *);
void init_ptr_arraylist(struct ptr_arraylist *, struct lox_allocator *);
void free_ptr_arraylist(struct ptr_arraylist *);

//Returns the index where it has been added
uint16_t append_ptr_arraylist(struct ptr_arraylist *, void * value);
void resize_ptr_arraylist(struct ptr_arraylist *, int new_size);
void clear_ptr_arraylist(struct ptr_arraylist *);