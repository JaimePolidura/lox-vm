#include "runtime/vm.h"

static void sleep_jit_native(uint64_t ms) {
    set_self_thread_waiting();
    sleep_ms(ms);
    set_self_thread_runnable();
}

static lox_value_t sleep_vm_native(int n_args, lox_value_t * args) {
    sleep_jit_native(AS_NUMBER(args[0]));
    return VOID_NATIVE_RETURN;
}

struct native_function_object sleep_native_function = (struct native_function_object) {
        .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
        .returned_type = PROFILE_DATA_TYPE_NIL,
        .native_vm_fn = sleep_vm_native,
        .native_jit_fn = sleep_jit_native,
        .name = "sleep"
};