#pragma once

#include "shared.h"
#include "scanner.h"
#include "bytecode.h"
#include "types/string_object.h"
#include "types/function.h"
#include "chunk/chunk_disassembler.h"
#include "compiler/compiler_structs.h"
#include "utils/utils.h"
#include "utils/trie.h"
#include "compiled_function.h"
#include "exported_symbol.h"
#include "package.h"
#include "types/package_object.h"

struct parser {
    struct token current;
    struct token previous;
    bool has_error;
};

struct local {
    struct token name;
    int depth;
};


struct compiler {
    struct package * package;
    scope_type_t scope;

    //This is set to true when the only input of the compiler is the code.
    //No local packages will be allowed to use
    //TODO Replace this with a enum
    bool is_standalone_mode;

    struct scanner * scanner;
    struct parser * parser;

    struct token current_variable_name; //Trick to get the current variable name which stores a struct

    struct compiled_function * current_function_in_compilation; //If scope is main, it points to package->main_function

    struct local locals[UINT8_MAX];
    int local_count;
    int local_depth;
};

struct compilation_result {
    struct package * compiled_package;
    bool success;
};

struct compilation_result compile(char * entrypoint_absolute_path, char * compilation_base_dir);

struct compilation_result compile_standalone(char * source_code);