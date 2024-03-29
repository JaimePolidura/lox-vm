#pragma once

#include "test.h"
#include "runtime/jit/x86/x86_jit_compiler.h"

static struct function_object * to_function(op_code first, ...);

// 1 + 2 - 3
TEST(x86_jit_compiler_simple_expression) {
    struct jit_compilation_result result = jit_compile(to_function(
            OP_CONST_1, OP_CONST_2, OP_ADD, OP_FAST_CONST_8, 3, OP_SUB, -1));

    for(int i = 0; i < result.compiled_code.in_use; i++){
        printf("%O2x", result.compiled_code.values[i]);
    }
}

static struct function_object * to_function(op_code first, ...) {
    struct function_object * function_object = alloc_function();

    va_list args;
    va_start(args, first);

    op_code current = first;
    while(current != -1){
        current = va_arg(args, op_code);
        write_chunk(&function_object->chunk, current, 1);
    }

    return function_object;
}