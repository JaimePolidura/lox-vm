#include "shared/types/native_function_object.h"
#include "runtime/threads/vm_thread.h"

extern __thread struct vm_thread * self_thread;

static lox_value_t self_thread_id_native(int n_args, lox_value_t * args) {
    return TO_LOX_VALUE_NUMBER(self_thread->thread_id);
}

struct native_function_object self_thread_id_native_function = (struct native_function_object) {
        .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
        .native_fn = self_thread_id_native,
        .name = "selfThreadId"
};