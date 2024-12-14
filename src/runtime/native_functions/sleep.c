#include "runtime/vm.h"

static lox_value_t sleep_native(int n_args, lox_value_t * args) {
    set_self_thread_waiting();

    sleep_ms(AS_NUMBER(args[0]));

    set_self_thread_runnable();

    return VOID_NATIVE_RETURN;
}

struct native_function_object sleep_native_function = (struct native_function_object) {
        .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
        .returned_type = PROFILE_DATA_TYPE_NIL,
        .native_fn = sleep_native,
        .name = "sleep"
};