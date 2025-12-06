#include "runtime/jit/jit_compiler.h"

static void jit_compile_jit_native(struct function_object * function) {
#ifdef NAN_BOXING
    try_jit_compile(function);
#endif
}

static lox_value_t jit_compile_vm_native(int n_args, lox_value_t * args) {
#ifdef NAN_BOXING
    jit_compile_jit_native((struct function_object *) AS_OBJECT(args[0]));
#endif

    return VOID_NATIVE_RETURN;
}

struct native_function_object force_jit_native_function = (struct native_function_object) {
        .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
        .returned_type = PROFILE_DATA_TYPE_NIL,
        .native_jit_fn = jit_compile_jit_native,
        .native_vm_fn = jit_compile_vm_native,
        .name = "forceJIT"
};