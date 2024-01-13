#pragma once

#include "types.h"
#include "string_object.h"
#include "chunk/chunk.h"

struct function_object {
    struct object object;
    int arity; //Number of arguments
    struct chunk chunk;
    struct string_object * name;
};

typedef enum {
    TYPE_FUNCTION_SCOPE,
    TYPE_MAIN_SCOPE,
} function_type_t;

struct function_object * alloc_function();

#define IS_FUNCTION(value) is_object_type(value, OBJ_FUNCTION)
#define FROM_GENERIC_TO_FUNCTION(value) ((struct function_object*) FROM_RAW_TO_OBJECT(value))
#define TO_FUNCTION(value) (struct function_object *) (value.as.object)