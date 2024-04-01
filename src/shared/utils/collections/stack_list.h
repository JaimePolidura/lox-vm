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

//TODO Replace names <operation>_stack to <operation>_stack_list

struct stack_list * alloc_stack_list();
void init_stack_list(struct stack_list * stack);

//TODO Replace name with free_stack
void clear_stack(struct stack_list * stack);

void push_stack(struct stack_list * stack, void * to_push);
void * pop_stack(struct stack_list * stack);
void * peek_stack_list(struct stack_list * stack);
bool is_empty(struct stack_list * stack);

