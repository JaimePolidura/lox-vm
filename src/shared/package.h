#pragma once

#include "shared.h"
#include "utils/collections/trie.h"
#include "compiler/bytecode/scanner.h"
#include "utils/substring.h"
#include "utils/utils.h"
#include "shared/utils/collections/lox_hash_table.h"

typedef enum {
    PENDING_COMPILATION, // Bytecode not generated
    PENDING_INITIALIZATION, // Bytecode generated but not executed
    INITIALIZING, // Executing lox package for the first time.
    READY_TO_USE // Bytecode generated & executed
} package_state_t;

struct package {
    //Package name. Defined by the name of the lox file
    const char * name;

    //Absolute path of package in the file system
    //Only set when package is local (not part of stdlib)
    //Only used in compile time
    const char * absolute_path;

    //Maintains mapping of names with global variables, this might include functions, packages, variables etc.
    //Only modified by the runtime when executing instruction OP_DEFINE_GLOBAL in scope SCOPE_PACKAGE (top level code)
    //This might be modified concurrently. lox_hash_table has rw mutex
    struct lox_hash_table global_variables;

    //Used at runtime & bytecode_compiler
    //This might be modified concurrently by runtime Protected by state_mutex
    package_state_t state;
    pthread_mutex_t state_mutex;

    struct function_object * main_function;

    //Includes functions, vars & structs
    //Only used and modified at compile time
    struct trie_list exported_symbols;

    //May include public & private
    //Only used and modified at compile time
    struct trie_list struct_definitions; //Stores struct_definition

    //May include public & private
    //Modified and used at compile time & maybe used in runtime
    struct lox_hash_table defined_functions;

    //Includes the name of const global variables defined in the package
    struct trie_list const_variables;

    uint32_t package_id;
};

void free_package(struct package * package);

struct package * alloc_package();
void init_package(struct package * package);

struct function_object * get_function_package(struct package *, char * function_name);

//Takes an import path and a length, and returns the package name.
//
//Packages names are included in the import path.
//An import could be in these types: math, math.lox, utils/math.lox
//This method should return math in these cases
struct substring read_package_name(char * import_name, int import_name_length);

char * read_package_source_code(char * absolute_path);

char * import_name_to_absolute_path(char * import_name, int import_name_length);