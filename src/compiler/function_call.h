#pragma once

#include "shared/package.h"

struct function_call {
    char * function_name;
    bool is_inlined;
    struct package * package;
    int call_bytecode_index;

    struct function_call * prev;
};

void init_function_calls(struct function_call *);
struct function_call * alloc_function_call();
