#include "utils.h"

int get_n_bytes_allocated_object(struct object * object) {
    switch (object->type) {
        case OBJ_ARRAY:
            struct array_object * array_object = (struct array_object *) object;
            int n_elements_rounded_up = round_up_8(array_object->values.capacity);
            return sizeof(struct array_object) + (n_elements_rounded_up * sizeof(lox_value_t));
        case OBJ_STRUCT_INSTANCE:
            return sizeof(struct struct_instance_object);
        case OBJ_STRING:
            return sizeof(struct string_object);
        default:
            return -1;
    }
}