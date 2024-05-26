#include "stack_list.h"

static struct stack_node * alloc_stack_node() {
    struct stack_node * node = malloc(sizeof(struct stack_node));
    node->next = NULL;
    node->data = NULL;
    node->prev = NULL;
    return node;
}

struct stack_list * alloc_stack_list() {
    struct stack_list * stack = malloc(sizeof(struct stack_list));
    init_stack_list(stack);
    return stack;
}

void * peek_stack_list(struct stack_list * stack) {
    return stack->head->data;
}

void free_stack_list(struct stack_list * stack) {
    struct stack_node * current_node = stack->head;
    while(current_node != NULL){
        struct stack_node * next_to_current = current_node->next;
        free(current_node);
        current_node = next_to_current;
    }

    stack->head = NULL;
}

void init_stack_list(struct stack_list * stack) {
    stack->head = NULL;
}

bool is_empty_stack_list(struct stack_list * stack) {
    return stack->head == NULL;
}

void push_n_stack_list(struct stack_list * stack_list, void * to_push, int n) {
    for (int i = 0; i < n; i++) {
        push_stack_list(stack_list, to_push);
    }
}

void push_stack_list(struct stack_list * stack, void * to_push) {
    struct stack_node * new_node = alloc_stack_node();
    struct stack_node * prev_node = stack->head;

    if(prev_node != NULL){
        prev_node->next = new_node;
    }

    new_node->prev = prev_node;
    new_node->data = to_push;
    stack->head = new_node;
}

void pop_n_stack_list(struct stack_list * stack, int n) {
    for (int i = 0; i < n; i++) {
        pop_stack_list(stack);
    }
}

void * pop_stack_list(struct stack_list * stack) {
    if(stack->head == NULL) { //Always return NULL in case the stack is emtpy
        return NULL;
    }

    struct stack_node * to_pop = stack->head;
    void * data_to_pop = to_pop->data;

    struct stack_node * new_head = to_pop->prev;

    stack->head = new_head;

    if(new_head != NULL){
        new_head->next = NULL;
    }

    free(to_pop);

    return data_to_pop;
}