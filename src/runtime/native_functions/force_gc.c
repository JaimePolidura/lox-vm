#include "runtime/memory/gc.h"

extern __thread struct vm_thread * self_thread;

static lox_value_t force_gc_native(int n_args, lox_value_t * args) {
    try_start_gc(self_thread->gc_info);

    return VOID_NATIVE_RETURN;
}

struct native_function_object force_gc_native_function = (struct native_function_object) {
    .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
    .native_fn = force_gc_native,
    .name = "forceGC"
};