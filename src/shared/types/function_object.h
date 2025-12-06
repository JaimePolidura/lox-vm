#pragma once

#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/trie.h"
#include "shared/types/function_scope.h"
#include "shared/types/string_object.h"
#include "shared/types/types.h"

#include "runtime/jit/jit_compilation_result.h"
#include "runtime/profiler/profile_data.h"
#include "runtime/threads/monitor.h"

#include "compiler/bytecode/bytecode_list.h"
#include "compiler/bytecode/chunk/chunk.h"

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

    //Mapping of local numbers to local variable names;
    struct u8_hash_table local_numbers_to_names;

    volatile function_state_t state;
    union {
        //Used when state is FUNC_STATE_NOT_PROFILING
        //This is just a counter of the function calls made to this function & the number of to instruction taken in this function
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
struct instruction_profile_data get_instruction_profile_data_function(struct function_object*, struct bytecode_list*);
struct type_profile_data get_function_argument_profiled_type(struct function_object *, int local_number);