#include "string_object.h"

struct string_object * copy_chars_to_string_object(const char * chars, int length) {
    struct string_object * string_object = malloc(sizeof(struct string_object));
    string_object->length = length;
    string_object->hash = hash_string(chars, length);
    string_object->chars = malloc(sizeof(char) * length + 1);
    memcpy(string_object->chars, chars, length);
    string_object->chars[length] = '\0';

    init_object(&string_object->object, OBJ_STRING);

    return string_object;
}