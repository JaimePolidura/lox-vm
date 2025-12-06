#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared/types/types.h"
#include "shared/utils/utils.h"
#include "shared.h"


struct lox_arraylist {
    //Don't change order, values struct member should always go first, since this might be used by the jit
    lox_value_t * values;
    struct lox_allocator * allocator;
    int capacity;
    int in_use;
};

void init_lox_arraylist(struct lox_arraylist *, struct lox_allocator *);
void init_lox_arraylist_with_size(struct lox_arraylist *, int size, struct lox_allocator *);

void append_lox_arraylist(struct lox_arraylist *, lox_value_t value);
void flip_lox_arraylist(struct lox_arraylist *);
void free_lox_arraylist(struct lox_arraylist *);