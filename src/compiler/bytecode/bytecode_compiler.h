#pragma once

#include "compiler/bytecode/chunk/chunk_disassembler.h"
#include "compiler/compilation_result.h"
#include "exported_symbol.h"
#include "function_call.h"
#include "compiler/bytecode/chunk/chunk.h"
#include "compiler/inline/call_graph.h"
#include "shared/bytecode/bytecode.h"
#include "scanner.h"

#include "shared/utils/collections/ptr_arraylist.h"
#include "shared/types/struct_definition_object.h"
#include "shared/types/struct_instance_object.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/trie.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/types/array_object.h"
#include "shared/types/string_object.h"
#include "shared/utils/substring.h"
#include "shared/utils/utils.h"
#include "shared/package.h"
#include "shared.h"

struct parser {
    struct token current;
    struct token previous;
    bool has_error;
};

struct local {
    struct token name;
    int depth;
};

//There will be one instance_node of struct bytecode_compiler per package being compiled
struct bytecode_compiler {
    struct package * package;
    struct function_object * current_function;

    //Indicates if we are compiling a function or top level code
    scope_type_t current_scope;

    //This is set to true when the only input of the bytecode_compiler is the code.
    //No local packages will be allowed to use
    bool is_standalone_mode;

    //After the compilation is done, this will get freed
    struct scanner * scanner;
    struct parser * parser;

    struct local locals[UINT8_MAX];
    int local_count; //Keeps track of latest local allocated
    int local_depth; //Keeps track of the scope depth
    int max_locals; //Nº Of max locals allocated

    monitor_number_t monitor_numbers_entered[MAX_MONITORS_PER_FUNCTION];
    monitor_number_t * last_monitor_entered;
    monitor_number_t last_monitor_allocated_number; //Increase-only monitor "id" to be able to indentify them

    char * current_function_call_name;

    //Indicates if the current function call/variable/struct is from an external package
    bool compiling_extermal_symbol_call;
    struct package * package_of_external_symbol;

    //Indicates if the current function call being compiled is parallel
    bool compiling_parallel_call;

    //Indicates if the current function call being compiled is inlined
    bool compiling_inline_call;

    //Keeps information of every function call even if it is repeated.
    struct function_call * function_calls;

    //Maintains function call list, so that we know how many different functions have been called
    //This is used by struct function_object field: n_function_calls
    //Strings inserted will be new heap allocated
    //After the compilation is done, this will get freed
    struct trie_list function_call_list;

    //May include public & private
    //This will be preserved after compilation, it will be stored in package.h
    struct lox_hash_table defined_functions;

    uint32_t next_package_id;

    //Trie list of const global variables declared in a package file
    //This will be preserved after compilation, it will be stored in package.h
    struct trie_list const_global_variables;
};

//compile_bytecode() takes the source code and returns the compiled bytecode.
//It doest do any optimizacions, inlining etc
//This is called by compiler.h which is the main entrypoint for compilation
struct compilation_result compile_bytecode(char * source_code, char * compiling_package_name, char * base_dir);
