#pragma once

#include "shared.h"
#include "chunk/chunk.h"
#include "utils/trie.h"
#include "scanner.h"

typedef enum {
    PENDING_COMPILATION, //Bytecode not generated
    PENDING_INITIALIZATION, //Bytecode generated but not executed
    READY_TO_USE //Bytecode generated & executed
} package_state_t;

struct package {
    char * name;

    //Used only for local imports
    char * absolute_path;

    int local_count;

    package_state_t state;

    struct function_object * main_function;

    //Includes functions, vars & structs
    struct trie_list exported_symbols;

    //May include public & private
    struct trie_list struct_definitions; //Stores struct_definition
};

struct package * alloc_package();
void init_package(struct package * package);

char * read_package_name(char * import_name, int import_name_length);
char * read_package_name_by_source_code(char * source_code);

char * read_package_source_code(char * import_name, int import_name_length);