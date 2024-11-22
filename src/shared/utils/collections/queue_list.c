#include "queue_list.h"

void init_queue_list(struct queue_list * queue) {
    init_ptr_arraylist(&queue->inner_list);
}

void free_queue_list(struct queue_list * queue) {
    free_ptr_arraylist(&queue->inner_list);
}

void enqueue_queue_list(struct queue_list * queue, void * to_enqueue) {
    append_ptr_arraylist(&queue->inner_list, to_enqueue);
}

void * dequeue_queue_list(struct queue_list * queue) {
    if(is_empty_queue_list(queue)){
        return NULL;
    }

    return queue->inner_list.values[--queue->inner_list.in_use];
}

bool is_empty_queue_list(struct queue_list * queue) {
    return queue->inner_list.in_use == 0;
}
