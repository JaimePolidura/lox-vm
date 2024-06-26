#pragma once

#include "compiler/bytecode/bytecode_list.h"
#include "compiler/bytecode/chunk/chunk_iterator.h"
#include "compiler/bytecode/chunk/chunk.h"

#include "shared/types/struct_definition_object.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/package.h"

struct function_inline_result {
    struct chunk * inlined_chunk;
    int n_constants_added;
    int total_size_added;
    int n_locals_added;
};

//This takes a target function and a function to inline. The function to inline will get "merged" into target
//at chunk_target_index index
struct function_inline_result inline_function(
        struct function_object * target,
        int chunk_target_index,
        struct function_object * to_inline,
        bool is_from_external_package
);