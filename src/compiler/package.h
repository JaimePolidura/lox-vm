#pragma once

#include "shared.h"
#include "chunk/chunk.h"
#include "utils/trie.h"
#include "scanner.h"
#include "utils/substring.h"
#include "utils/utils.h"

typedef enum {
    PENDING_COMPILATION, //Bytecode not generated
    PENDING_INITIALIZATION, //Bytecode generated but not executed
    INITIALIZING,
    READY_TO_USE //Bytecode generated & executed
} package_state_t;

struct package {
    char * name;

    //Used only for local imports
    char * absolute_path;

    //Used only by vm
    struct hash_table global_variables;

    int local_count;

    package_state_t state;

    struct function_object * main_function;

    //Includes functions, vars & structs
    struct trie_list exported_symbols;

    //May include public & private
    struct trie_list struct_definitions; //Stores struct_definition
};

void free_package(struct package * package);

struct package * alloc_package();
void init_package(struct package * package);

// Takes an import path and a length, and returns the package name.
//
// Packages names are included in the import path.
// An import could be in these types: math, math.lox, utils/math.lox
// This method should return math in these cases
struct substring read_package_name(char * import_name, int import_name_length);

char * read_package_source_code(char * absolute_path);

char * import_name_to_absolute_path(char * import_name, int import_name_length);