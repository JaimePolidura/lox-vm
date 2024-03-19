#pragma once

#include "runtime/threads/monitor.h"
#include "string_object.h"
#include "compiler/chunk/chunk.h"
#include "shared/utils/collections/trie.h"
#include "types.h"

#define MAX_MONITORS_PER_FUNCTION 8

typedef enum {
    BYTECODE,
    JIT_COMPILING,
    JIT_COMPILED
} function_state;

struct function_object {
    struct object object;
    int n_arguments; //Number of arguments
    struct chunk chunk;
    struct string_object * name;

    volatile function_state state;

    //Only jit compilation in this function will occur when n_function_calls == n_function_calls_jit_compiled
    int n_function_calls; //Number of different function calls. This includes local functions & package functions
    int n_function_calls_jit_compiled; //Number of n_function_calls that haven been jit compiled

    //Number of times that this function has been called. Will be incremented atomically by multiple threads
    int n_times_called;

    struct monitor monitors[MAX_MONITORS_PER_FUNCTION];
};

typedef enum {
    SCOPE_FUNCTION,
    SCOPE_PACKAGE,
} scope_type_t;

struct function_object * alloc_function();
