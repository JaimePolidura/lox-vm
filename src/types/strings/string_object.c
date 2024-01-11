#include "string_object.h"

struct string_object * copy_chars_to_string_object(const char * chars, int length) {
    struct string_object * string_object = malloc(sizeof(struct string_object *));
    string_object->object.type = OBJ_STRING;
    string_object->length = length;
    string_object->hash = hash_string(chars, length);
    string_object->chars = malloc(sizeof(char) * length + 1);
    memcpy(string_object->chars, chars, length);
    string_object->chars[length] = '\0';
    
    return string_object;
}

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