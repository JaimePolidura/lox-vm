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

void free_stack_list(struct stack_list * stack);

void push_stack_list(struct stack_list *, void * to_push);
void push_n_stack_list(struct stack_list *, void * to_push, int n);
void * pop_stack_list(struct stack_list *);
void pop_n_stack_list(struct stack_list *, int n);
void * peek_stack_list(struct stack_list *);
bool is_empty_stack_list(struct stack_list *);

