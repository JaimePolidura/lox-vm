#pragma once

#include "test.h"
#include "runtime/jit/x64/x64_jit_compiler.h"

#define CONTAINS_DWORD(function_ptr) \
    (function_ptr >> 0) & 0xFF,  \
    (function_ptr >> 8) & 0xFF,  \
    (function_ptr >> 16) & 0xFF, \
    (function_ptr >> 24) & 0xFF \

#define CONTAINS_QWORD(function_ptr) \
    (function_ptr >> 0) & 0xFF,  \
    (function_ptr >> 8) & 0xFF,  \
    (function_ptr >> 16) & 0xFF, \
    (function_ptr >> 24) & 0xFF, \
    (function_ptr >> 32) & 0xFF, \
    (function_ptr >> 40) & 0xFF, \
    (function_ptr >> 48) & 0xFF, \
    (function_ptr >> 56) & 0xFF \

static struct function_object * to_function(op_code first, ...);
static void print_jit_result(struct jit_compilation_result result);

extern struct struct_instance_object * alloc_struct_instance_object();
extern void print_lox_value(lox_value_t value);

TEST(x64_jit_compiler_structs_get){
    struct struct_instance_object * instance = alloc_struct_instance_object();
    struct string_object * field_name = alloc_string_object("x");

    struct function_object * function = to_function(OP_CONSTANT, 0, OP_GET_STRUCT_FIELD, 1, OP_EOF);
    add_constant_to_chunk(&function->chunk, TO_LOX_VALUE_OBJECT(instance)); //Constant 0
    add_constant_to_chunk(&function->chunk, TO_LOX_VALUE_OBJECT(field_name)); //Constant 1

    struct jit_compilation_result result = jit_compile(function);

    ASSERT_U8_SEQ(result.compiled_code.values,
                  0x55, // push rbp
                  0x48, 0x89, 0xe5, //mov rbp, rsp
                  0x49, 0xbf, CONTAINS_QWORD(TO_LOX_VALUE_OBJECT(instance)), // movabs r15, instance pointer OP_CONST 0 instance pointers
                  0x49, 0xbe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, //movabs r14, 0x3ffffffffffff (~(FLOAT_SIGN_BIT | QUIET_FLOAT_NAN)) cast instance ptr to lox
                  0x4d, 0x21, 0xf7, //and r15, r14
                  0x49, 0x83, 0xc7, 0x10, //add r15, 10
                  0x48, 0x83, 0xec, 0x08, //sub rsp, 8 (allocate space for get_hash_table 3rd param)
                  0x41, 0x51, //push r9 36
                  0x49, 0xb9, CONTAINS_QWORD((uint64_t) &get_hash_table),
                  0x57, 0x56, 0x52, //push rdi, rsi, rdx
                  0x4c, 0x89, 0xff, //mov rdi, r15
                  0x48, 0xc7, 0xc6, CONTAINS_DWORD((uint64_t) field_name),
                  0x48, 0x8b, 0x55, 0xf8, //mov rdx, [rbp - 8]
                  0x41, 0xff, 0xd1, //call r9
                  0x5a, 0x5e, 0x5f, 0x41, 0x59, //pop rdx, rsi, rdi, ri
                  0x4c, 0x8b, 0x7d, 0xf8, //mov r15, [rbp - 8]
                  0x48, 0x83, 0xc4, 0x08, //add rsp, 8
    );
}

TEST(x64_jit_compiler_structs_set) {
    struct struct_instance_object * instance = alloc_struct_instance_object();
    struct string_object * field_name = alloc_string_object("x");

    struct function_object * function = to_function(OP_CONSTANT, 0, OP_CONST_1, OP_SET_STRUCT_FIELD, 1, OP_EOF);
    add_constant_to_chunk(&function->chunk, TO_LOX_VALUE_OBJECT(instance)); //Constant 0
    add_constant_to_chunk(&function->chunk, TO_LOX_VALUE_OBJECT(field_name)); //Constant 1

    struct jit_compilation_result result = jit_compile(function);

    ASSERT_U8_SEQ(result.compiled_code.values,
                  0x55, // push rbp
                  0x48, 0x89, 0xe5, //mov rbp, rsp
                  0x49, 0xbf, CONTAINS_QWORD(TO_LOX_VALUE_OBJECT(instance)), // movabs r15, instance pointer OP_CONST 0 instance pointers
                  0x49, 0xc7, 0xc6, 0x01, 0x00, 0x00, 0x00, //mov r14, 1 (OP_CONST 1)
                  0x49, 0xbd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, //movabs r13, 0x3ffffffffffff (~(FLOAT_SIGN_BIT | QUIET_FLOAT_NAN)) cast instance ptr to lox
                  0x4d, 0x21, 0xef, // and r15, r13
                  0x49, 0x83, 0xc7, (uint8_t) offsetof(struct struct_instance_object, fields), //add r15, <field names offset>
                  0x41, 0x51, //push r9
                  0x49, 0xb9, CONTAINS_QWORD((uint64_t) &put_hash_table), //movabs r9, put_hash_table (function ptr)
                  0x57, 0x56, 0x52, //push rdi, rsi, rdx
                  0x4c, 0x89, 0xff, //mov rdi, r15
                  0x48, 0xc7, 0xc6, CONTAINS_DWORD((uint64_t) field_name), //mov  rsi, field_name
                  0x4c, 0x89, 0xf2, // mov rdx,r14
                  0x41, 0xff, 0xd1, //call r9
                  );
}

TEST(x64_jit_compiler_structs_initialize) {
    struct struct_definition_object * struct_definition = alloc_struct_definition_object();
    struct_definition->n_fields = 2;
    struct_definition->field_names = malloc(sizeof(struct token) * 2);
    struct_definition->field_names[0] = alloc_string_object("x");
    struct_definition->field_names[1] = alloc_string_object("y");

    struct function_object * function = to_function(
            OP_CONST_1, OP_CONST_2, OP_INITIALIZE_STRUCT, 0, OP_SET_LOCAL, 0, //Initilizae struct var = struct{1, 2}
            OP_EOF
            );
    function->n_arguments = 1;
    add_constant_to_chunk(&function->chunk, TO_LOX_VALUE_OBJECT(struct_definition));

    struct jit_compilation_result result = jit_compile(function);

    ASSERT_U8_SEQ(result.compiled_code.values,
                  0x55, // push rbp
                  0x48, 0x89, 0xe5, //mov rbp, rsp
                  0x48, 0x83, 0xec, 0x08, //sub rsp, 16 Increase stack size by 1
                  0x49, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, //mov r15, 1
                  0x49, 0xc7, 0xc6, 0x02, 0x00, 0x00, 0x00, //mov r14, 2 (OP_CONST_1, OP_CONST_2) Struct initialization fields
                  0x41, 0x51, //push r9
                  0x49, 0xb9, CONTAINS_QWORD((uint64_t) &alloc_struct_instance_object), //movabs r9, alloc_struct_instance_object
                  0x41, 0xff, 0xd1, //call r9
                  0x41, 0x59, //pop r9 (call to alloc_struct_instance_object)
                  0x48, 0x83, 0xc0, (uint8_t) offsetof(struct struct_instance_object, fields), // add rax, <field names offset>
                  0x50, //push rax
                  //CODE FOR SETTING ONE FIELD OF STRUCT
                  0x41, 0x51, //push r9 prepare call to put_hash_table
                  0x49, 0xb9, CONTAINS_QWORD((uint64_t) &put_hash_table), //movabs r9, put_hash_table
                  0x57, 0x56, 0x52, //push rdi, rsi, rdx
                  0x48, 0x89, 0xc7, //push rdi, rax load first argument (struct_instance fields member address)
                  0x48, 0xc7, 0xc6, CONTAINS_DWORD((uint64_t) struct_definition->field_names[1]),
                  0x4c, 0x89, 0xf2, //mov rdx, r14 // y value 2
                  0x41, 0xff, 0xd1, //call r9 (call to put_hash_table)
                  0x5a, 0x5e, 0x5f, 0x41, 0x59, //pop rdx, rsi, rdi, r9
                  0x58 //pop rax
                  );
}

TEST(x64_jit_compiler_for_loop) {
    struct function_object * function = to_function(
            OP_FAST_CONST_8, 0, OP_SET_LOCAL, 0, OP_POP, //i = 0
            OP_GET_LOCAL, 0, OP_FAST_CONST_8, 5, OP_LESS, OP_JUMP_IF_FALSE, 0, 15, //i < 5 jump to OP_NO_OP (note we start counting from OP_GET_LOCAL)
            OP_GET_LOCAL, 0, OP_SET_LOCAL, 1, OP_POP,  //var = i;
            OP_GET_LOCAL, 0, OP_CONST_1, OP_ADD, OP_SET_LOCAL, 0, OP_POP, //i++
            OP_LOOP, 0, 23, //Jump to i < 5 OP_GET_LOCAL (2º line) (we start counting from OP_NO_OP)
            OP_NO_OP,
            OP_EOF
    );
    function->n_arguments = 2; //Adjust rsp to the number of locals

    struct jit_compilation_result result = jit_compile(function);

    ASSERT_U8_SEQ(result.compiled_code.values,
                  0x55, // push rbp
                  0x48, 0x89, 0xe5, //mov rbp, rsp
                  0x48, 0x83, 0xec, 0x10, //sub rsp, 16 Increase stack size by 2

                  0x49, 0xc7, 0xc7, 0x00, 0x00, 0x00, 0x00, //mov r15, 0 (OP_FAST_CONST_8, 0)
                  0x4c, 0x89, 0x7d, 0x00, //mov [rbp+0x0], r15 (OP_SET_LOCAL, 0)
                  0x4c, 0x8b, 0x7d, 0x00, //mov r15, [rbp+0x0] (OP_GET_LOCAL, 0)
                  0x49, 0xc7, 0xc6, 0x05, 0x00, 0x00, 0x00, //mov r14, 0x05 (OP_FAST_CONST_8, 5) 29

                  //(OP_LESS) We compare both values and cast it to lox boolean
                  0x4d, 0x39, 0xfe, //cmp r14, r15
                  0x0f, 0x9c, 0xc0, //sete al
                  0x4c, 0x0f, 0xb6, 0xf8, //movzx r15, al
                  0x49, 0x83, 0xc7, 0x02, //add r15, 0x2
                  0x49, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, //movabs r14, TRUE_VALUE
                  0x4d, 0x09, 0xf7, //or r15, r14 56

                  //(OP_JUMP_IF_FALSE)
                  0x49, 0xbe, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, //movabs r14, TRUE_VALUE
                  0x4d, 0x39, 0xfe, //cmp r14, r15
                  0x0f, 0x85, 0x1f, 0x00, 0x00, 0x00, //jne <offset> index: 75

                  0x4c, 0x8b, 0x7d, 0x00, // mov r15, [rbp] (OP_GET_LOCAL, 0)
                  0x4c, 0x89, 0x7d, 0x08, // mov [rbp + 8], r15 (OP_SET_LOCAL, 1) 83
                  0x4c, 0x8b, 0x7d, 0x00, // mov r15, [rbp] (OP_GET_LOCAL, 0)
                  0x49, 0xc7, 0xc6, 0x01, 0x00, 0x00, 0x00, //mov r14, 1, (OP_CONST_1)
                  0x4d, 0x01, 0xf7, //add r15, r14 (OP_ADD)
                  0x4c, 0x89, 0x7d, 0x00, //mov [rbp], r15 (OP_SET_LOCAL, 0)
                  0xe9, 0xa8, 0xff, 0xff, 0xff, //jmp -88 (OP_LOOP, 0, 23)
                  0x90 //nop (107)
    );
}

TEST(x64_jit_compiler_division_multiplication){
    //(1 * 2) / 1
    struct jit_compilation_result result = jit_compile(to_function(OP_CONST_1, OP_CONST_2, OP_MUL, OP_CONST_1, OP_DIV, OP_EOF));

    ASSERT_U8_SEQ(result.compiled_code.values,
                  0x55, // push rbp
                  0x48, 0x89, 0xe5, //mov rbp, rsp
                  0x49, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, //mov r15, 1
                  0x49, 0xc7, 0xc6, 0x02, 0x00, 0x00, 0x00, //mov r14, 2
                  0x4d, 0x0f, 0xaf, 0xfe, // imul r15, r14
                  0x49, 0xc7, 0xc6, 0x01, 0x00, 0x00, 0x00, //mov r14, 1
                  0x4c, 0x89, 0xf8, //mov rax, r15
                  0x49, 0xf7, 0xfe, //idiv r14
                  0x49, 0x89, 0xc7, //mov r15,rax
    );
}

TEST(x64_jit_compiler_negation) {
    struct jit_compilation_result result = jit_compile(to_function(OP_CONST_1, OP_CONST_2, OP_EQUAL,
            OP_NEGATE, OP_NOT, OP_EOF));

    ASSERT_U8_SEQ(result.compiled_code.values,
                  0x55, // push rbp
                  0x48, 0x89, 0xe5, //mov rbp, rsp
                  0x49, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, //mov r15, 1
                  0x49, 0xc7, 0xc6, 0x02, 0x00, 0x00, 0x00, //mov r14, 2
                  0x4d, 0x39, 0xfe, //cmp r14, r15 OP_EQUAL
    );
}

// 1 + 2 - 3
TEST(x64_jit_compiler_simple_expression) {
    struct jit_compilation_result result = jit_compile(to_function(OP_CONST_1, OP_CONST_2, OP_ADD,
            OP_FAST_CONST_8, 3, OP_SUB, OP_PRINT, OP_EOF));

    uint64_t print_ptr = (uint64_t) &print_lox_value;

    ASSERT_U8_SEQ(result.compiled_code.values,
                  0x55, // push rbp
                  0x48, 0x89, 0xe5, //mov rbp, rsp
                  0x49, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, // mov r15, 1
                  0x49, 0xc7, 0xc6, 0x02, 0x00, 0x00, 0x00, // mov r14, 2
                  0x4d, 0x01, 0xf7, // add r15, r14
                  0x49, 0xc7, 0xc6, 0x03, 0x00, 0x00, 0x00, // mov r14, 3
                  0x4d, 0x29, 0xf7, // sub r15, r14
                  0x41, 0x51, //push r9
                  0x49, 0xb9, CONTAINS_QWORD(print_ptr), //movabs r9, <print function address>
                  0x57, //push rdi
                  0x4c, 0x89, 0xff, //mov rdi, r15
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