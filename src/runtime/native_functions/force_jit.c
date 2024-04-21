#include "runtime/jit/jit_compiler.h"

static lox_value_t jit_compile_native(int n_args, lox_value_t * args) {
    struct function_object * function_object = (struct function_object *) AS_OBJECT(args[0]);

    try_jit_compile(function_object);

    return VOID_NATIVE_RETURN;
}

struct native_function_object force_jit_native_function = (struct native_function_object) {
        .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
        .native_fn = jit_compile_native,
        .name = "forceJIT"
};