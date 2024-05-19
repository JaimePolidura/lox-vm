#pragma once

#include "runtime/threads/monitor.h"
#include "string_object.h"
#include "compiler/chunk/chunk.h"
#include "shared/utils/collections/trie.h"
#include "types.h"
#include "runtime/jit/jit_info.h"
#include "compiler/function_call.h"

#define MAX_MONITORS_PER_FUNCTION 8

struct function_object {
    struct object object;
    int n_arguments; //Number of arguments
    struct chunk chunk;
    struct string_object * name;
    int n_locals;
    struct function_call * function_calls;

    struct jit_info jit_info;

    struct monitor monitors[MAX_MONITORS_PER_FUNCTION];
};

typedef enum {
    SCOPE_FUNCTION,
    SCOPE_PACKAGE,
} scope_type_t;

struct function_object * alloc_function();