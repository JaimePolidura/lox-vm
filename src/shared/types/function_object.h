#pragma once

#include "runtime/threads/monitor.h"
#include "string_object.h"
#include "compiler/bytecode/chunk/chunk.h"
#include "shared/utils/collections/trie.h"
#include "types.h"
#include "runtime/jit/jit_compilation_result.h"
#include "function_scope.h"
#include "runtime/profiler/profile_data.h"

#define MAX_MONITORS_PER_FUNCTION 8

typedef enum function_state {
    FUNC_STATE_NOT_PROFILING,
    FUNC_STATE_PROFILING,
    FUNC_STATE_JIT_COMPILING,
    FUNC_STATE_JIT_COMPILED,
    FUNC_STATE_JIT_UNCOMPILABLE
} function_state_t;

struct function_object {
    struct object object;
    int n_arguments; //Number of arguments
    struct chunk * chunk;
    struct string_object * name;
    int n_locals;
    struct function_call * function_calls;

    volatile function_state_t state;
    union {
        //Used when state is FUNC_STATE_NOT_PROFILING
        //This is just a counter of the function calls made to this function & the number of branch instruction taken in this function
        //This is speciallly useful in loops
        struct {
            int n_calls;
        } not_profiling;

        //Used when state is FUNC_STATE_PROFILING
        //This includes a datastructure to profile each instruction in the function
        struct {
            struct function_profile_data profile_data;
        } profiling;

        //Used when state is FUNC_STATE_JIT_COMPILED
        //This includes the function jit compiled code
        struct {
            volatile jit_compiled code;
        } jit_compiled;
    } state_as;

    struct monitor monitors[MAX_MONITORS_PER_FUNCTION];
};

struct function_object * alloc_function();