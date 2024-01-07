#include "native.h"

struct native_object * alloc_native_object(native_fn native_fn) {
    struct native_object * native_object = ALLOCATE_OBJ(struct native_object, OBJ_NATIVE);
    native_object->native_fn = native_fn;
    return native_object;
}