#pragma once

#include "test.h"
#include "runtime/jit/x64/x64_jit_compiler.h"

static struct function_object * to_function(op_code first, ...);

TEST(x86_jit_compiler_negation) {
    struct jit_compilation_result result = jit_compile(to_function(OP_CONST_1, OP_CONST_2, OP_EQUAL,
            OP_NEGATE, OP_NOT, OP_EOF));
    
    for(int i = 0; i < result.compiled_code.in_use; i++){
        uint8_t a = result.compiled_code.values[i];

        printf("%02X", result.compiled_code.values[i]);
    }

    puts("\n");
}

// 1 + 2 - 3
TEST(x86_jit_compiler_simple_expression) {
    struct jit_compilation_result result = jit_compile(to_function(OP_CONST_1, OP_CONST_2, OP_ADD,
            OP_FAST_CONST_8, 3, OP_SUB, OP_PRINT, OP_EOF));

    ASSERT_U8_SEQ(result.compiled_code.values,
                  0x49, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, // mov $0x1, %r15
                  0x49, 0xc7, 0xc6, 0x02, 0x00, 0x00, 0x00, // mov $0x2, %r14
                  0x4d, 0x01, 0xfe, // add %r15, %r14
                  0x49, 0xc7, 0xc6, 0x03, 0x00, 0x00, 0x00, // mov $0x3, %r14
                  0x4d, 0x29, 0xfe, // sub %r15, %r14
                  );
}

static struct function_object * to_function(op_code first, ...) {
    struct function_object * function_object = alloc_function();

    va_list args;
    va_start(args, first);

    op_code current = first;
    while(current != OP_EOF) {
        write_chunk(&function_object->chunk, current, 1);
        current = va_arg(args, op_code);
    }

    write_chunk(&function_object->chunk, OP_EOF, 1);

    return function_object;
}