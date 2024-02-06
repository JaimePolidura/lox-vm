#pragma once

#include "shared.h"
#include "chunk/chunk.h"
#include "utils/trie.h"
#include "scanner.h"

typedef enum {
    PENDING_COMPILATION,
    PENDING_INITIALIZATION,
    READY_TO_USE
} package_state_t;

struct package {
    char * name;

    package_state_t state;

    struct function_object * main_function;

    //Includes functions, vars & structs
    struct trie_list exported_symbols;

    //May include public & private
    struct trie_list struct_definitions; //Stores struct_definition
};

struct package * alloc_package();
void init_package(struct package * package);

char * read_package_name(struct token import_path);