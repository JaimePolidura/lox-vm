#include "register_allocator.h"

//As VM is stack based, we can treat register allocation as a stack.

void init_register_allocator(struct register_allocator * register_allocation) {
    register_allocation->next_free_register = R15;
}

register_t allocate_register(struct register_allocator * register_allocation) {
    if(register_allocation->next_free_register < 0){
        return -1;
    }

    return register_allocation->next_free_register--;
}

void free_register(struct register_allocator * register_allocation) {
    register_allocation->next_free_register--;
}