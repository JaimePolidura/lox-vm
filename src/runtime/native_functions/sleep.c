#include "runtime/vm.h"

static lox_value_t sleep_native(int n_args, lox_value_t * args) {
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

struct native_function_object sleep_native_function = (struct native_function_object) {
        .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
        .native_fn = sleep_native,
        .name = "sleep"
};