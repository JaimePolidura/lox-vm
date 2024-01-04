#include "string_object.h"


char * cast_to_string(lox_value_t value) {
    switch(value.type) {
        case VAL_BOOL: return value.as.boolean ? "true" : "false";
        case VAL_NIL: return "";
        case VAL_NUMBER: {
            int size = snprintf(NULL, 0, "%f", value.as.number);
            char * str = malloc(size);
            snprintf(str, size, "%f", value.as.number);
            return str;
        }
        case VAL_OBJ:
            switch(value.as.object->type){
                case OBJ_STRING: return TO_STRING(value)->chars;
            }
    }

    return NULL;
}