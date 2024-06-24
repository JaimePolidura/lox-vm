#include "runtime/vm.h"

extern __thread struct vm_thread * self_thread;

static bool join_thread(struct vm_thread * parent_ignored, struct vm_thread * thread, int index_ignored, void * extra) {
    pthread_join(thread->native_thread, NULL);
    return true;
}

static lox_value_t join_native(int n_args, lox_value_t * args) {
    set_self_thread_waiting();

    for_each_thread(self_thread, join_thread, NULL, THREADS_OPT_EXCLUSIVE | THREADS_OPT_RECURSIVE);

    set_self_thread_runnable();

    return VOID_NATIVE_RETURN;
}

struct native_function_object join_native_function = (struct native_function_object) {
        .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
        .native_fn = join_native,
        .name = "join"
};