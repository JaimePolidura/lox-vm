#pragma once

#include "shared.h"

/**
 *      prev
 * head --> other node
 *
 *      next
 * head <-- other node
 */

struct stack_list {
    struct stack_node * head;
};

struct stack_node {
    struct stack_node * next;
    struct stack_node * prev;
    void * data;
};

struct stack_list * alloc_stack_list();
void init_stack_list(struct stack_list * stack);

void clear_stack(struct stack_list * stack);

void push_stack(struct stack_list * stack, void * to_push);
void * pop_stack(struct stack_list * stack);