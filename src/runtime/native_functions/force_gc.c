#include "runtime/native_functions/native_function_definer.h"
#include "runtime/memory/gc_result.h"

extern struct gc_result try_start_gc_alg(lox_value_t * args);

extern __thread struct vm_thread * self_thread;

//TOOD Improve
static void force_gc_jit_native(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    uint64_t args[4];
    args[0] = arg1;
    args[1] = arg2;
    args[2] = arg3;
    args[3] = arg4;
    try_start_gc_alg((lox_value_t *) &args);
}

static lox_value_t force_gc_vm_native(int n_args, lox_value_t * args) {
    try_start_gc_alg(args);
    return VOID_NATIVE_RETURN;
}

struct native_function_object force_gc_native_function = (struct native_function_object) {
    .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
    .returned_type = PROFILE_DATA_TYPE_NIL,
    .native_jit_fn = force_gc_jit_native,
    .native_vm_fn = force_gc_vm_native,
    .name = "forceGC"
};