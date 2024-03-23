#pragma once

#include "shared.h"
#include "registers.h"

struct register_allocator {
    register_t next_free_register;
};

void init_register_allocator(struct register_allocator * register_allocation);

register_t allocate_register(struct register_allocator * register_allocation);
void free_register(struct register_allocator * register_allocation);