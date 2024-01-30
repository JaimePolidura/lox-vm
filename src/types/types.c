#include "types.h"
#include "string_object.h"

bool cast_to_boolean(lox_value_t value) {
    return value.as.number != 0;
}

struct object * allocate_object(size_t size, object_type_t type) {
    struct object * object = (struct object *) malloc(size);
    object->gc_marked = false;
    object->type = type;
    object->next = NULL; //Should be linked in vm

    return object;
}

char string_as_double[20];

char * to_string(lox_value_t value) {
    switch (value.type) {
        case VAL_NIL: return "nil"; break;
        case VAL_NUMBER: {
            sprintf(string_as_double, "%f", value.as.number);
            return string_as_double;
        }
        case VAL_BOOL: return value.as.boolean ? "true" : "false"; break;
        case VAL_OBJ:
            switch (value.as.object->type) {
                case OBJ_STRING: return TO_STRING_CHARS(value);
            }
    };

    perror("Cannot print to string");
    exit(-1);
}