#include "jit_stack.h"

void push_operand_jit_stack(struct jit_stack * jit_stack, struct operand operand) {
    switch (operand.type) {
        case IMMEDIATE_OPERAND:
            push_immediate_jit_stack(jit_stack, operand.as.immediate);
            break;
        case REGISTER_OPERAND:
            push_register_jit_stack(jit_stack, operand.as.reg);
            break;
        case REGISTER_DISP_OPERAND:
            push_displacement_jit_stack(jit_stack, operand.as.reg_disp.reg, operand.as.reg_disp.disp);
            break;
    }
}

void push_immediate_jit_stack(struct jit_stack * jit_stack, uint64_t number) {
    jit_stack->items[jit_stack->in_use++] = TO_IMMEDIATE_JIT_STACK_ITEM(number);
}

void push_heap_allocated_register_jit_stack(struct jit_stack * jit_stack, register_t reg) {
    struct jit_stack_item jit_stack_item =  TO_REGISTER_JIT_STACK_ITEM(reg);
    jit_stack_item.is_heap_allocated = true;
    jit_stack->items[jit_stack->in_use++] = jit_stack_item;
}

void push_register_jit_stack(struct jit_stack * jit_stack, register_t reg) {
    jit_stack->items[jit_stack->in_use++] = TO_REGISTER_JIT_STACK_ITEM(reg);
}

void push_native_stack_jit_stack(struct jit_stack * jit_stack) {
    jit_stack->items[jit_stack->in_use++] = TO_NATIVE_STACK_JIT_STACK_ITEM();
}

void push_displacement_jit_stack(struct jit_stack * jit_stack, register_t reg, uint64_t disp) {
    jit_stack->items[jit_stack->in_use++] = TO_DISPLACEMENT_JIT_STACK_ITEM(DISPLACEMENT_TO_OPERAND(reg, disp));
}

struct jit_stack_item pop_jit_stack(struct jit_stack * jit_stack) {
    return jit_stack->items[--jit_stack->in_use];
}

void init_jit_stack(struct jit_stack * jit_stack) {
    jit_stack->in_use = 0;
}

int n_heap_allocations_in_jit_stack(struct jit_stack * jit_stack) {
    int n_heap_allocations = 0;

    for(int i = 0; i < jit_stack->in_use; i++){
        if(jit_stack->items[i].is_heap_allocated){
            n_heap_allocations++;
        }
    }

    return n_heap_allocations;
}