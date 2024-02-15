#pragma once

#include "shared.h"
#include "scanner.h"
#include "bytecode.h"
#include "types/string_object.h"
#include "types/function.h"
#include "chunk/chunk_disassembler.h"
#include "utils/utils.h"
#include "utils/trie.h"
#include "exported_symbol.h"
#include "package.h"
#include "types/package_object.h"
#include "utils/substring.h"
#include "types/struct_definition_object.h"
#include "types/struct_instance_object.h"

struct parser {
    struct token current;
    struct token previous;
    bool has_error;
};

struct local {
    struct token name;
    int depth;
};

typedef enum {
    COMPILER_NONE,
    COMPILER_COMPILING_EXTERNAL_FUNCTION,
} compiler_state_t;

struct compiler {
    struct package * package;
    scope_type_t scope;

    char * source_code;

    //This is set to true when the only input of the compiler is the code.
    //No local packages will be allowed to use
    //TODO Replace this with a enum
    bool is_standalone_mode;

    struct scanner * scanner;
    struct parser * parser;

    struct function_object * current_function_in_compilation;

    struct local locals[UINT8_MAX];
    int local_count;
    int local_depth;

    compiler_state_t state;
    struct package * package_of_compiling_external_func;
};

struct compilation_result {
    struct package * compiled_package;
    char * error_message;
    int local_count;
    bool success;
};

struct compilation_result compile(char * entrypoint_absolute_path, char * compilation_base_dir, char * package_name_entrypoint);

struct compilation_result compile_standalone(char * source_code);