#pragma once

#include "shared.h"
#include "registers.h"
#include "shared/utils/utils.h"

#define SELF_THREAD_ADDR_REG RBX
#define LOX_ESP_REG RCX
#define LOX_EBP_REG RDX
#define LOX_ESP_REG_OPERAND REGISTER_TO_OPERAND(LOX_ESP_REG)
#define LOX_EBP_REG_OPERAND REGISTER_TO_OPERAND(LOX_EBP_REG)

/**
 * Register allocation:
 *
 * RAX used for:
 * - Storing the division result,
 * - Storing in low bytes AL the boolean result of a comparation
 * - Storing the return value of external C function calls
 *
 * RCX used for storing the previous RSP register
 * RDX used for storing the previous RBP register
 * RBX used for storing the self_thread pointer. This is assigned in runtime
 * RBP, RSP used for stack managment wether it is lox stack or native stack
 *
 * RSI to R15 are used for register allocation
*/

struct register_allocator {
    register_t next_free_register;
    int n_allocated_registers;
};

void init_register_allocator(struct register_allocator *);

//Allocates new register
register_t push_register_allocator(struct register_allocator *);

//Deallocates latest allocated register
register_t pop_register_allocator(struct register_allocator *);

void pop_at_register_allocator(struct register_allocator *, int items);

register_t peek_register_allocator(struct register_allocator *);

register_t peek_at_register_allocator(struct register_allocator *, int index);
