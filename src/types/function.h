#pragma once

#include "types.h"
#include "string_object.h"
#include "chunk/chunk.h"
#include "compiler/compiler_structs.h"

struct function_object {
    struct object object;
    int n_arguments; //Number of arguments
    struct chunk chunk;
    struct string_object * name;
    struct struct_instance * struct_instances; //Only used for compilation
};

struct struct_instance {
    struct token name;
    struct struct_definition * struct_definition;
    struct struct_instance * next;
};

typedef enum {
    TYPE_FUNCTION_SCOPE,
    TYPE_MAIN_SCOPE,
} function_type_t;

struct struct_instance * alloc_struct_compilation_instance();
struct function_object * alloc_function();

#define IS_FUNCTION(value) is_object_type(value, OBJ_FUNCTION)
#define FROM_GENERIC_TO_FUNCTION(value) ((struct function_object*) FROM_RAW_TO_OBJECT(value))
#define TO_FUNCTION(value) (struct function_object *) (value.as.object)