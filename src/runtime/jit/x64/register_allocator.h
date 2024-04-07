#pragma once

#include "shared.h"
#include "registers.h"

struct register_allocator {
    register_t next_free_register;
};

void init_register_allocator(struct register_allocator * register_allocation);

//Allocates new register
register_t push_register_allocator(struct register_allocator * register_allocation);

//Deallocates latest allocated register
register_t pop_register_allocator(struct register_allocator * register_allocation);

register_t peek_register_allocator(struct register_allocator * register_allocation);

register_t peek_at_register_allocator(struct register_allocator * register_allocation, int index);
