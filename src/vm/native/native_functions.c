#include "native_functions.h"

extern __thread struct vm_thread * self_thread;
extern void set_self_thread_runnable();
extern void set_self_thread_waiting();

lox_value_t clock_native(int n_args, lox_value_t * args) {
    return TO_LOX_VALUE_NUMBER(1000000); //TODO Example
}

static void join_thread(struct vm_thread * parent_ignored, struct vm_thread * thread, int index_ignored) {
    pthread_join(thread->native_thread, NULL);
}

lox_value_t join_native(int n_args, lox_value_t * args) {
    set_self_thread_waiting();

    for_each_thread(self_thread, join_thread, THREADS_OPT_EXCLUSIVE | THREADS_OPT_RECURSIVE);

    set_self_thread_runnable();

    return TO_LOX_VALUE_NUMBER(0);
}

lox_value_t sleep_ms_native(int n_args, lox_value_t * args) {
    long milliseconds = AS_NUMBER(args[0]);
    struct timespec ts = {
            .tv_nsec = (milliseconds % 1000) * 1000000,
            .tv_sec = milliseconds / 10000
    };

    set_self_thread_waiting();

    nanosleep(&ts, NULL);

    set_self_thread_runnable();

    return TO_LOX_VALUE_NUMBER(0);
}

lox_value_t self_thread_id_native(int n_args, lox_value_t * args) {
    return TO_LOX_VALUE_NUMBER(self_thread->thread_id);
}
