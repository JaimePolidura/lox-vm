#include "types.h"
#include "string_object.h"

bool cast_to_boolean(lox_value_t value) {
#ifdef NAN_BOXING
    return AS_BOOL(value);
#else
    return value.as.number != 0;
#endif
}

void init_object(struct object * object, object_type_t type) {
    object->type = type;
    object->next = NULL;
    object->gc_marked = false;
}

struct object * allocate_object(size_t size, object_type_t type) {
    struct object * object = (struct object *) malloc(size);
    init_object(object, type);

    return object;
}

//This current_function should be only used for testing since it may leak memory when value is a number
char * to_string(lox_value_t value) {
#ifdef NAN_BOXING
    if(IS_NIL(value)) {
        return "nil";
    } else if(IS_OBJECT(value)) {
        struct object * object = AS_OBJECT(value);

        switch (AS_OBJECT(value)->type) {
            case OBJ_STRING: {
                return ((struct string_object *) AS_OBJECT(value))->chars;
            }
        }
    } else if(IS_BOOL(value)) {
        return AS_OBJECT(value) ? "true" : "false";
    } else if(IS_NUMBER(value)) {
        char * string_as_double = malloc(sizeof(char) * 20);
        sprintf(string_as_double, "%f", AS_NUMBER(value));
        return string_as_double;
    }
#else
    switch (value.type) {
        case VAL_NIL: return "nil"; break;
        case VAL_NUMBER: {
            char * string_as_double = malloc(sizeof(char) * 20);
            sprintf(string_as_double, "%f", value.as.number);
            return string_as_double;
        }
        case VAL_BOOL: return value.as.boolean ? "true" : "false"; break;
        case VAL_OBJ:
            switch (value.as.object->type) {
                case OBJ_STRING: return AS_STRING_CHARS_OBJECT(value);
            }
    };
#endif
    perror("Cannot print");
    exit(-1);
}