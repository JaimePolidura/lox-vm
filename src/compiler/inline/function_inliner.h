#pragma once

#include "compiler/bytecode/chunk/chunk_iterator.h"
#include "compiler/bytecode/chunk/chunk.h"
#include "shared/types/function_object.h"

struct function_inline_result {
    struct chunk inlined_chunk;
    int total_size_added;
};

struct function_inline_result inline_function(struct function_object * target, int chunk_target_index,
        struct function_object * function_to_inline);