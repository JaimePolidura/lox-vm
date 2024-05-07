#pragma once

#include "runtime/jit/x64/register_allocator.h"
#include "runtime/threads/vm_thread.h"
#include "runtime/jit/x64/opcodes.h"

#include "shared/utils/collections/u8_arraylist.h"
#include "shared/types/function_object.h"

#include "shared.h"

//Used for simulating vm stack, when compiling

#define TO_DISPLACEMENT_JIT_STACK_ITEM(a) (struct jit_stack_item) {.type = DISPLACEMENT_JIT_STACK_ITEM, {.displacement = a}}
#define TO_IMMEDIATE_JIT_STACK_ITEM(a) (struct jit_stack_item) {.type = IMMEDIATE_JIT_STACK_ITEM, {.immediate = a}}
#define TO_NATIVE_STACK_JIT_STACK_ITEM() (struct jit_stack_item) {.type = NATIVE_STACK_JIT_STACK_ITEM, {.reg = 0}}
#define TO_REGISTER_JIT_STACK_ITEM(a) (struct jit_stack_item) {.type = REGISTER_JIT_STACK_ITEM, {.reg = a}}

struct jit_stack_item {
    enum {
        NATIVE_STACK_JIT_STACK_ITEM,
        DISPLACEMENT_JIT_STACK_ITEM,
        IMMEDIATE_JIT_STACK_ITEM,
        REGISTER_JIT_STACK_ITEM,
    } type;
    union {
        uint64_t immediate;
        register_t reg;
        struct operand displacement;
    } as;
};

struct jit_stack {
    struct jit_stack_item items[STACK_MAX];
    int in_use;
};

void push_operand_jit_stack(struct jit_stack *, struct operand);

void push_immediate_jit_stack(struct jit_stack *, uint64_t number);
void push_register_jit_stack(struct jit_stack *, register_t reg);
void push_native_stack_jit_stack(struct jit_stack *);
void push_displacement_jit_stack(struct jit_stack *, register_t reg, uint64_t disp);

struct jit_stack_item pop_jit_stack(struct jit_stack *);

void init_jit_stack(struct jit_stack * jit_stack);

bool does_single_pop_vm_stack(op_code opcode);