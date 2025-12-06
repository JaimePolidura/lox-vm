#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared/types/function_scope.h"
#include "shared/package.h"

//When we compile a function and that functions calls another function, we store that profile_data.
//We store if the call is inlined, the other function name and package and the pending_bytecode index of that call
struct function_call {
    char * function_name;
    bool is_inlined;
    struct package * package;
    int call_bytecode_index;

    struct function_call * prev;
};

void init_function_calls(struct function_call *);
struct function_call * alloc_function_call();

int get_n_function_calls(struct function_call *);