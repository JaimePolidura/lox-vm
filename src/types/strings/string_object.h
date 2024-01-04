#pragma once

#include "../../shared.h"

#include "../types.h"

struct string_object {
    struct object object;
    int length;
    uint32_t hash;
    char * chars;
};

struct string_object * copy_chars_to_string_object(const char * chars, int length);

char * cast_to_string(lox_value_t value);

#define TO_STRING(value) ((struct string_object *)TO_OBJECT(value))
#define TO_STRING_CHARS(value) (((struct string_object *) value.as.object)->chars)

#define IS_STRING(value) is_object_type(value, OBJ_STRING)

static inline bool is_object_type(lox_value_t value, object_type_t type) {
    return IS_OBJECT(value) && TO_OBJECT(value)->type == type;
}
