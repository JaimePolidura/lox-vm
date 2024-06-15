#include "vm_thread.h"

static void init_vm_thread(struct vm_thread * vm_thread);

struct vm_thread * alloc_vm_thread() {
    struct vm_thread * vm_thread = malloc(sizeof(struct vm_thread));
    init_vm_thread(vm_thread);
    return vm_thread;
}

static void init_vm_thread(struct vm_thread * vm_thread) {
    vm_thread->thread_id = -1;
    vm_thread->parent = NULL;
    vm_thread->gc_info = NULL;
    vm_thread->esp = vm_thread->stack;
    vm_thread->current_package = NULL;
    vm_thread->frames_in_use = 0;
    vm_thread->terminated_state = THREAD_TERMINATED_NONE;

    init_stack_list(&vm_thread->package_stack);
    
    for(int i = 0; i < MAX_THREADS_PER_THREAD; i++){
        vm_thread->children[i] = NULL;
    }
}

void free_vm_thread(struct vm_thread * vm_thread) {
}

void for_each_thread(struct vm_thread * start_thread, thread_consumer_t callback, void * extra, long options) {
    struct stack_list pending;
    init_stack_list(&pending);
    push_stack_list(&pending, start_thread);

    if(!(options & THREADS_OPT_EXCLUSIVE)){
        callback(NULL, start_thread, 0, extra);
    }

    while(!is_empty_stack_list(&pending)) {
        struct vm_thread * current = pop_stack_list(&pending);

        for(int i = 0; i < MAX_THREADS_PER_THREAD; i++) {
            struct vm_thread * child_of_current = current->children[i];
            bool not_null = child_of_current != NULL;
            bool is_terminated = not_null && child_of_current->state == THREAD_TERMINATED;
            bool include_terminated_option = options & THREADS_OPT_INCLUDE_TERMINATED;

            if(not_null && (!is_terminated || (include_terminated_option && is_terminated))){
                callback(current, child_of_current, i, extra);

                if(!(options & THREADS_OPT_NOT_RECURSIVE)){
                    push_stack_list(&pending, child_of_current);
                }
            }
        }
    }
}

struct call_frame * get_current_frame_vm_thread(struct vm_thread * thread) {
    return &thread->frames[thread->frames_in_use - 1];
}