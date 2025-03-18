#include "runtime/vm.h"

extern __thread struct vm_thread * self_thread;

static bool join_thread(struct vm_thread * parent_ignored, struct vm_thread * thread, int index_ignored, void * extra) {
    pthread_join(thread->native_thread, NULL);
    return true;
}

static void join_jit_native() {
    set_self_thread_waiting();
    for_each_thread(self_thread, join_thread, NULL, THREADS_OPT_EXCLUSIVE | THREADS_OPT_RECURSIVE);
    set_self_thread_runnable();
}

static lox_value_t join_vm_native(int n_args, lox_value_t * args) {
    join_jit_native();
    return VOID_NATIVE_RETURN;
}

struct native_function_object join_native_function = (struct native_function_object) {
        .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
        .returned_type = PROFILE_DATA_TYPE_NIL,
        .native_jit_fn = join_jit_native,
        .native_vm_fn = join_vm_native,
        .name = "join"
};