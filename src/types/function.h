#pragma once

#include "types.h"
#include "strings/string_object.h"
#include "chunk/chunk.h"

struct function_object {
    struct object object;
    int arity;
    struct chunk * chunk;
    struct string_object * name;
};

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} function_type_t;

struct function_object * alloc_function();

#define IS_FUNCTION(value) is_object_type(value, OBJ_FUNCTION)
#define FROM_FUNCTION(value) ((struct function_object*) FROM_OBJECT(value))