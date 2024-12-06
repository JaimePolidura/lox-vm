#include "string_object.h"

struct string_object * copy_chars_to_string_object(const char * chars, int length) {
    struct string_object * string_object = ALLOCATE_OBJ(struct string_object, OBJ_STRING);
    string_object->object.type = OBJ_STRING;
    string_object->length = length;
    string_object->hash = hash_string(chars, length);
    string_object->chars = NATIVE_LOX_MALLOC(sizeof(char) * length + 1);
    memcpy(string_object->chars, chars, length);
    string_object->chars[length] = '\0';

    return string_object;
}

struct string_object * alloc_string_object(char * str) {
    return copy_chars_to_string_object(str, strlen(str));
}
