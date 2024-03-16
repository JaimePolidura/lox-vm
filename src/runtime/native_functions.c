#include "native_functions.h"

extern __thread struct vm_thread * self_thread;
extern struct vm current_vm;

extern void set_self_thread_runnable();
extern void set_self_thread_waiting();

#define VOID_NATIVE_RETURN TO_LOX_VALUE_NUMBER(0)

lox_value_t clock_native(int n_args, lox_value_t * args) {
    return TO_LOX_VALUE_NUMBER(time_millis());
}

static void join_thread(struct vm_thread * parent_ignored, struct vm_thread * thread, int index_ignored, void * extra) {
    pthread_join(thread->native_thread, NULL);
}

lox_value_t join_native(int n_args, lox_value_t * args) {
    set_self_thread_waiting();

    for_each_thread(self_thread, join_thread, NULL, THREADS_OPT_EXCLUSIVE | THREADS_OPT_RECURSIVE);

    set_self_thread_runnable();

    return VOID_NATIVE_RETURN;
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

    return VOID_NATIVE_RETURN;
}

lox_value_t self_thread_id_native(int n_args, lox_value_t * args) {
    return TO_LOX_VALUE_NUMBER(self_thread->thread_id);
}

lox_value_t force_gc(int n_args, lox_value_t * args) {
    try_start_gc(self_thread->gc_info);

    return VOID_NATIVE_RETURN;
}