#include "native_function_object.h"

struct native_function_object * alloc_native_object(native_vm_fn native_vm_fn, native_jit_fn native_jit_fn, char * name) {
    struct native_function_object * native_object = ALLOCATE_OBJ(struct native_function_object, OBJ_NATIVE_FUNCTION);
    init_object(&native_object->object, OBJ_NATIVE_FUNCTION);
    native_object->native_jit_fn = native_jit_fn;
    native_object->native_vm_fn = native_vm_fn;
    native_object->name = name;
    return native_object;
}