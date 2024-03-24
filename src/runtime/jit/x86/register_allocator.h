#pragma once

#include "shared.h"
#include "registers.h"

//As VM is stack based, we can treat register allocation as a stack.
//RAX & RCX registers are not used for allocations
struct register_allocator {
    register_t next_free_register;
};

void init_register_allocator(struct register_allocator * register_allocation);

register_t push_register(struct register_allocator * register_allocation);

//Returns last register allocated, which is the one being freed
register_t pop_register(struct register_allocator * register_allocation);