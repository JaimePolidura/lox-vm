#pragma once

#include "types.h"
#include "string_object.h"
#include "chunk/chunk.h"
#include "compiler/compiler_structs.h"
#include "utils/trie.h"

struct function_object {
    struct object object;
    int n_arguments; //Number of arguments
    struct chunk chunk;
    struct string_object * name;

    //Mapping between variable names and struct_definition
    //This is only used in compile time
    struct trie_list struct_instances;
};

typedef enum {
    SCOPE_FUNCTION,
    SCOPE_PACKAGE,
} scope_type_t;

struct struct_instance * alloc_struct_compilation_instance();
struct function_object * alloc_function();
