#pragma once

#include "shared.h"
#include "scanner.h"
#include "bytecode.h"
#include "types/string_object.h"
#include "types/function.h"
#include "chunk/chunk_disassembler.h"
#include "compiler/compiler.h"

struct compilation_result {
    struct function_object * function_object;
    struct chunk * chunk;
    bool success;
};

struct compilation_result compile(char * source_code);
