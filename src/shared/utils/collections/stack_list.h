#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared.h"
#include "u64_set.h"

// prev:
// head -> other control_node_to_lower
//
// next:
// head <- other control_node_to_lower
struct stack_list {
    struct lox_allocator * allocator;
    struct stack_node * head;
};

struct stack_node {
    struct stack_node * next;
    struct stack_node * prev;
    void * data;
};

struct stack_list * alloc_stack_list(struct lox_allocator *);
void init_stack_list(struct stack_list *, struct lox_allocator *);
void free_stack_list(struct stack_list *);

void push_stack_list(struct stack_list *, void * to_push);
void push_n_stack_list(struct stack_list *, void * to_push, int n);
void push_set_stack_list(struct stack_list *, struct u64_set);
void * pop_stack_list(struct stack_list *);
void pop_n_stack_list(struct stack_list *, int n);
void * peek_stack_list(struct stack_list *);
void * peek_n_stack_list(struct stack_list *, int n);
bool is_empty_stack_list(struct stack_list *);