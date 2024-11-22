#pragma once

#include "shared.h"
#include "shared/utils/collections/ptr_arraylist.h"

struct queue_list {
    struct ptr_arraylist inner_list;
};

struct queue_list * alloc_queue_list();
void init_queue_list(struct queue_list * stack);
void free_queue_list(struct queue_list * stack);

void enqueue_queue_list(struct queue_list *, void * to_enqueue);
void * dequeue_queue_list(struct queue_list *);
bool is_empty_queue_list(struct queue_list *);
