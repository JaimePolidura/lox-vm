#pragma once

#include "shared.h"
#include "registers.h"

//As VM is stack based, we can treat register allocation as a stack.
struct register_allocator {
    register_t next_free_register;
};

void init_register_allocator(struct register_allocator * register_allocation);

register_t push_register_allocator(struct register_allocator * register_allocation);

register_t pop_register_allocator(struct register_allocator * register_allocation);

register_t peek_register_allocator(struct register_allocator * register_allocation);
