#include "register_allocator.h"

const register_t register_to_allocate[] = {
        RSI, // 0110 Register number:  6
        RDI, // 0111 Register number:  7
        R8,  // 1000 Register number:  8
        R9,  // 1001 Register number:  9
        R10, // 1010 Register number: 10
        R11, // 1101 Register number: 11
        R12, // 1100 Register number: 12
        R13, // 1101 Register number: 13
        R14, // 1110 Register number: 14
        R15  // 1111 Register number: 15
};

void init_register_allocator(struct register_allocator * register_allocation) {
    register_allocation->next_free_register = R15;
}

register_t push_register(struct register_allocator * register_allocation) {
    if(register_allocation->next_free_register < register_to_allocate[0]){
        return -1;
    }

    return register_allocation->next_free_register--;
}

register_t pop_register(struct register_allocator * register_allocation) {
    return register_allocation->next_free_register++;
}