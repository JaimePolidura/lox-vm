#pragma once

#include "test.h"
#include "runtime/jit/x64/x64_jit_compiler.h"

static struct function_object * to_function(op_code first, ...);
static void print_jit_result(struct jit_compilation_result result);
extern void print_lox_value(lox_value_t value);

TEST(x64_jit_compiler_negation) {
    struct jit_compilation_result result = jit_compile(to_function(OP_CONST_1, OP_CONST_2, OP_EQUAL,
            OP_NEGATE, OP_NOT, OP_EOF));

    print_jit_result(result);
}

// 1 + 2 - 3
TEST(x64_jit_compiler_simple_expression) {
    struct jit_compilation_result result = jit_compile(to_function(OP_CONST_1, OP_CONST_2, OP_ADD,
            OP_FAST_CONST_8, 3, OP_SUB, OP_PRINT, OP_EOF));

    uint64_t print_ptr = (uint64_t)  &print_lox_value;

    ASSERT_U8_SEQ(result.compiled_code.values,
                  0x55, // push rbp
                  0x48, 0x89, 0xe5, //mov rbp, rsp
                  0x49, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, // mov r15, 1
                  0x49, 0xc7, 0xc6, 0x02, 0x00, 0x00, 0x00, // mov r14, 2
                  0x4d, 0x01, 0xf7, // add r15, r14
                  0x49, 0xc7, 0xc6, 0x03, 0x00, 0x00, 0x00, // mov r14, 3
                  0x4d, 0x29, 0xf7, // sub r15, r14
                  0x41, 0x51, //push r9
                  0x49, 0xb9,
                    (print_ptr >> 0) & 0xFF,
                    (print_ptr >> 8) & 0xFF,
                    (print_ptr >> 16) & 0xFF,
                    (print_ptr >> 24) & 0xFF,
                    (print_ptr >> 32) & 0xFF,
                    (print_ptr >> 40) & 0xFF,
                    (print_ptr >> 48) & 0xFF,
                    (print_ptr >> 56) & 0xFF, //movabs r9, <print function address>
                  0x57, //push rdi
                  0x49, 0x89, 0xff, //mov r15, rdi
                  0x41, 0xff, 0xd1, //call r9
                  0x5f, //pop rdi
                  0x41, 0x59, //pop r9
                  0x5d, //pop rbp
                  0xc3, //ret
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

static void print_jit_result(struct jit_compilation_result result) {
    for(int i = 0; i < result.compiled_code.in_use; i++){
        uint8_t a = result.compiled_code.values[i];

        printf("%02X", result.compiled_code.values[i]);
    }

    puts("\n");
}