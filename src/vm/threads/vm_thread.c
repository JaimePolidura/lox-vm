#include "vm_thread.h"

struct vm_thread * alloc_vm_thread() {
    struct vm_thread * vm_thread = malloc(sizeof(struct vm_thread));
    init_vm_thread(vm_thread);
    return vm_thread;
}

void init_vm_thread(struct vm_thread * vm_thread) {
    vm_thread->state = NEW;
    vm_thread->thread_id = -1;

    vm_thread->esp = vm_thread->stack;

    vm_thread->current_package = NULL;
    init_stack_list(&vm_thread->package_stack);

    vm_thread->frames_in_use = 0;

    for(int i = 0; i < MAX_CHILD_THREADS_PER_THREAD; i++){
        vm_thread->children[i] = NULL;
    }
}