#pragma once

#include "shared.h"
#include "scanner.h"
#include "bytecode.h"
#include "types/string_object.h"
#include "types/function.h"
#include "chunk/chunk_disassembler.h"
#include "compiler/compiler_structs.h"

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
    struct compiler * parent; // Used for functions
    struct compiler_struct * structs; //Inherited from parent. Linked list of structs definitions
    struct scanner * scanner;
    struct parser * parser;
    struct function_object * function;
    function_type_t function_type;
    struct local locals[UINT8_MAX];
    int local_count;
    int local_depth;
};

struct compilation_result {
    struct function_object * function_object;
    struct chunk * chunk;
    bool success;
};

struct compilation_result compile(char * source_code);
