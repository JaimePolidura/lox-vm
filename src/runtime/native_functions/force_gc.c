#include "runtime/native_functions/native_function_definer.h"
#include "runtime/memory/gc_result.h"

extern struct gc_result try_start_gc_alg();

extern __thread struct vm_thread * self_thread;

static lox_value_t force_gc_native(int n_args, lox_value_t * args) {
    try_start_gc_alg();
    return VOID_NATIVE_RETURN;
}

struct native_function_object force_gc_native_function = (struct native_function_object) {
    .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
    .native_fn = force_gc_native,
    .name = "forceGC"
};