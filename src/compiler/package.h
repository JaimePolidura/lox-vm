#pragma once

#include "shared.h"
#include "chunk/chunk.h"
#include "utils/trie.h"
#include "scanner.h"

struct package {
    char * name;

    struct chunk chunk;

    //Includes functions, vars & functions
    struct trie_list exported_symbols;

    //May include public & private structs
    struct trie_list struct_definitions;
};

struct package * alloc_package();
void init_package(struct package * package);

char * read_package_name(struct token import_path);