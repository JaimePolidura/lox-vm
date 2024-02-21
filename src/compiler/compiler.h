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
#include "utils/stack_list.h"

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

    struct function_object * current_function;

    // Indicates if we are compiling a function or top level code
    scope_type_t current_scope;

    // This is set to true when the only input of the compiler is the code.
    // No local packages will be allowed to use
    bool is_standalone_mode;

    struct scanner * scanner;
    struct parser * parser;

    struct local locals[UINT8_MAX];
    int local_count;
    int local_depth;

    bool compiling_external_function;
    struct package * package_of_compiling_external_func;

    bool compiling_parallel_call;
};

struct compilation_result {
    struct package * compiled_package;
    char * error_message;
    int local_count;
    bool success;
};

struct compilation_result compile(char * entrypoint_absolute_path, char * compilation_base_dir, char * package_name_entrypoint);

struct compilation_result compile_standalone(char * source_code);