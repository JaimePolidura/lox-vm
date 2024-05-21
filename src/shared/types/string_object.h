#pragma once

#include "shared.h"

#include "shared/utils/utils.h"
#include "types.h"

struct string_object {
    struct object object;
    int length;
    uint32_t hash;
    char * chars;
};

struct string_object * copy_chars_to_string_object(const char * chars, int length);
//Only used for testing
struct string_object * alloc_string_object(char *);

#define AS_STRING_OBJECT(value) ((struct string_object *)AS_OBJECT(value))
#define AS_STRING_CHARS_OBJECT(value) (((struct string_object *) AS_OBJECT(value))->chars)
#define IS_STRING(value) is_object_type(value, OBJ_STRING)

static inline bool is_object_type(lox_value_t value, object_type_t type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}
