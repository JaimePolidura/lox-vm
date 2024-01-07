#include "types.h"

bool cast_to_boolean(lox_value_t value) {
    return value.as.number != 0;
}

struct object * allocate_object(size_t size, object_type_t type) {
    struct object * object = (struct object *) reallocate(NULL, 0, size);
    object->type = type;
    object->next = NULL; //Should be linked in vm

    return object;
}