#pragma once

#include "shared.h"
#include "scanner.h"
#include "bytecode.h"
#include "shared/types/string_object.h"
#include "shared/types/function_object.h"
#include "chunk/chunk_disassembler.h"
#include "shared/utils/utils.h"
#include "shared/utils/collections/trie.h"
#include "exported_symbol.h"
#include "shared/package.h"
#include "shared/types/package_object.h"
#include "shared/utils/substring.h"
#include "shared/types/struct_definition_object.h"
#include "shared/types/struct_instance_object.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/array_object.h"
#include "chunk/chunk.h"

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

    int monitor_depth;

    //Maintains function call list, so that we know how many different functions have been called
    //This is used by struct function_object field: n_function_calls
    //Strings inserted will be new heap allocated
    struct trie_list function_call_list;
};

struct compilation_result {
    struct package * compiled_package;
    char * error_message;
    int local_count;
    bool success;
};

struct compilation_result compile(char * entrypoint_absolute_path, char * compilation_base_dir, char * package_name_entrypoint);

struct compilation_result compile_standalone(char * source_code);