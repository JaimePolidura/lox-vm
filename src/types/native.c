#include "native.h"

struct native_object * alloc_native_object(native_fn native_fn, bool is_blocking) {
    struct native_object * native_object = ALLOCATE_OBJ(struct native_object, OBJ_NATIVE);
    native_object->is_blocking = is_blocking;
    native_object->native_fn = native_fn;
    return native_object;
}