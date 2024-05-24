#pragma once

#include "shared/types/function_object.h"
#include "compiler/bytecode/chunk/chunk.h"

struct function_inline_result {
    struct chunk inlined_chunk;
    int total_size_added;
};

struct function_inline_result inline_function(struct function_object * target, int chunk_target_index,
        struct function_object * function_to_inline);