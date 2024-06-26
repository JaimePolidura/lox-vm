#include "utils.h"

int get_n_bytes_allocated_object(struct object * object) {
    switch (object->type) {
        case OBJ_ARRAY:
            struct array_object * array = (struct array_object *) object;
            return align(sizeof(struct array_object) + (array->values.in_use * sizeof(lox_value_t)), sizeof(struct object));
        case OBJ_STRUCT_INSTANCE:
            return align(sizeof(struct struct_instance_object), sizeof(struct object));
        case OBJ_STRING:
            struct string_object * string = (struct string_object *) object;
            return align(sizeof(struct string_object) + string->length + 1, sizeof(struct object));
        default:
            return -1;
    }
}