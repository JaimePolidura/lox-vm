#include "register_allocator.h"

void init_register_allocator(struct register_allocator * register_allocation) {
    register_allocation->next_free_register = R15;
}

register_t push_register(struct register_allocator * register_allocation) {
    if(register_allocation->next_free_register >= RCX){
        return -1;
    }

    return register_allocation->next_free_register--;
}

register_t pop_register(struct register_allocator * register_allocation) {
    return register_allocation->next_free_register++;
}