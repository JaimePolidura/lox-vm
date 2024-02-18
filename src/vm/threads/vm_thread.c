#include "vm_thread.h"

static void init_vm_thread(struct vm_thread * vm_thread);

struct vm_thread * alloc_vm_thread() {
    struct vm_thread * vm_thread = malloc(sizeof(struct vm_thread));
    init_vm_thread(vm_thread);
    return vm_thread;
}

static void init_vm_thread(struct vm_thread * vm_thread) {
    vm_thread->thread_id = -1;

    init_gc_thread_info(&vm_thread->gc_info);

    vm_thread->esp = vm_thread->stack;
    vm_thread->signaled_when_sleeping = true;
    vm_thread->current_package = NULL;
    init_stack_list(&vm_thread->package_stack);

    vm_thread->frames_in_use = 0;

    for(int i = 0; i < MAX_CHILD_THREADS_PER_THREAD; i++){
        vm_thread->children[i] = NULL;
    }
}

static void free_vm_thread_recursive(struct vm_thread * vm_thread) {
    for(int i = 0; i < MAX_CHILD_THREADS_PER_THREAD; i++){
        struct vm_thread * current_thread = vm_thread->children[i];

        if(current_thread != NULL){
            free_vm_thread_recursive(current_thread);
        }
    }

    free(vm_thread);
}

void free_vm_thread(struct vm_thread * vm_thread) {
    for(int i = 0; i < MAX_CHILD_THREADS_PER_THREAD; i++){
        struct vm_thread * current_thread = vm_thread->children[i];

        if(current_thread != NULL){
            free_vm_thread_recursive(current_thread);
        }
    }
}

void for_each_thread(struct vm_thread * start_thread, consumer_t callback) {
    struct stack_list pending;
    init_stack_list(&pending);
    push_stack(&pending, start_thread);

    while(!is_empty(&pending)){
        struct vm_thread * current = pop_stack(&pending);

        for(int i = 0; i < MAX_CHILD_THREADS_PER_THREAD; i++){
            struct vm_thread * child_of_current = current->children[i];

            if(child_of_current != NULL && child_of_current->state != THREAD_TERMINATED){
                callback(child_of_current);
                push_stack(&pending, child_of_current);
            }
        }
    }
}