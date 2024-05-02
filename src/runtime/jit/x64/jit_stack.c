#include "jit_stack.h"

void push_immediate_jit_stack(struct jit_stack * jit_stack, uint64_t number) {
    jit_stack->items[jit_stack->in_use++] = TO_IMMEDIATE_JIT_STACK_ITEM(number);
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

bool does_single_pop_vm_stack(op_code opcode) {
    switch (opcode) {
        case OP_POP:
        case OP_RETURN:
        case OP_NEGATE:
        case OP_PRINT:
        case OP_DEFINE_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_SET_LOCAL:
        case OP_NOT:
        case OP_JUMP_IF_FALSE:
        case OP_GET_STRUCT_FIELD:
        case OP_ENTER_PACKAGE:
        case OP_GET_ARRAY_ELEMENT:
            return true;
        default:
            return false;
    }
}