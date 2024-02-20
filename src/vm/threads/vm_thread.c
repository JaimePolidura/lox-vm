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

void for_each_thread(struct vm_thread * start_thread, thread_consumer_t callback, long options) {
    struct stack_list pending;
    init_stack_list(&pending);
    push_stack(&pending, start_thread);

    if(!(options & THREADS_OPT_EXCLUSIVE)){
        callback(NULL, start_thread, 0);
    }

    while(!is_empty(&pending)) {
        struct vm_thread * current = pop_stack(&pending);

        for(int i = 0; i < MAX_CHILD_THREADS_PER_THREAD; i++) {
            struct vm_thread * child_of_current = current->children[i];
            bool not_null = child_of_current != NULL;
            bool is_terminated = not_null && child_of_current->state == THREAD_TERMINATED;
            bool include_terminated_option = options & THREADS_OPT_INCLUDE_TERMINATED;

            if(not_null && (!is_terminated || (include_terminated_option && is_terminated))){
                callback(current, child_of_current, i);

                if(!(options & THREADS_OPT_NOT_RECURSIVE)){
                    push_stack(&pending, child_of_current);
                }
            }
        }
    }
}