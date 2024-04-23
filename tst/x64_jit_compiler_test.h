#pragma once

#include "test.h"
#include "runtime/jit/x64/x64_jit_compiler.h"

#define QWORD(function_ptr) \
    (function_ptr >> 0) & 0xFF,  \
    (function_ptr >> 8) & 0xFF,  \
    (function_ptr >> 16) & 0xFF, \
    (function_ptr >> 24) & 0xFF, \
    (function_ptr >> 32) & 0xFF, \
    (function_ptr >> 40) & 0xFF, \
    (function_ptr >> 48) & 0xFF, \
    (function_ptr >> 56) & 0xFF \

//push rbp
//mov rbp, rsp
#define PROLOGUE \
    0x55,                    \
    0x48, 0x89, 0xe5

//mov rcx, rsp
//mov rdx, rbp
//mov rsp, [rbx + 0xa20]
//mov rbp, [rbx + 0xa20]
//sub rbp, 8
#define SETUP_VM_TO_JIT_MODE \
    0x48, 0x89, 0xe1, \
    0x48, 0x89, 0xea, \
    0x48, 0x8b, 0xa3, 0x20, 0x0a, 0x00, 0x00, \
    0x48, 0x8b, 0xab, 0x20, 0x0a, 0x00, 0x00, \
    0x48, 0x83, 0xed, 0x08

//mov rcx, rsp
//mov rdx, rbp
//mov rsp, [rbx + 0xa20]
//mov rbp, [rbx + 0xa20]
//add rsp, n*8
//sub rbp, 8
#define SETUP_VM_TO_JIT_MODE_WITH_ARGS(n)  \
    0x48, 0x89, 0xe1, \
    0x48, 0x89, 0xea, \
    0x48, 0x8b, 0xa3, 0x20, 0x0a, 0x00, 0x00,  \
    0x48, 0x8b, 0xab, 0x20, 0x0a, 0x00, 0x00,  \
    0x48, 0x83, 0xed, n * 8, \
    0x48, 0x83, 0xed, 0x08

//mov r14,rbx
//mov r14, [r14 + 0xa28]
//mov [r14 + 0x0], rsp
//mov [r14 + 0x0], rbp
//mov [r14 + 0x10], rbx
//mov rsp,rcx
//mov rbp,rdx
//push r14
#define SWITCH_JIT_TO_NATIVE_MODE \
    0x49, 0x89, 0xde, \
    0x4d, 0x8b, 0xb6, 0x28, 0x0a, 0x00, 0x00, \
    0x49, 0x89, 0x66, 0x00, \
    0x49, 0x89, 0x6e, 0x08, \
    0x49, 0x89, 0x5e, 0x10, \
    0x48, 0x89, 0xcc, \
    0x48, 0x89, 0xd5, \
    0x41, 0x56

//pop rbq
//ret
#define EPILOGUE 0x5d, 0xc3

//mov rcx, rsp
//mov rbx, rbp
//pop r14
//mov rsp, [r14 + 0x0]
//mov rbp, [r14 + 0x8]
//mov rbx, [r14 + 0x10]
#define SWITCH_NATIVE_TO_JIT_MODE \
    0x48, 0x89, 0xe1, \
    0x48, 0x89, 0xeb, \
    0x41, 0x5e, \
    0x49, 0x8b, 0x66, 0x00, \
    0x49, 0x8b, 0x6e, 0x08, \
    0x49, 0x8b, 0x5e, 0x10

static struct function_object * to_function(op_code first, ...);
static void print_jit_result(struct jit_compilation_result result);

extern struct struct_instance_object * alloc_struct_instance_object();
extern void print_lox_value(lox_value_t value);
extern void check_gc_on_safe_point_alg();
extern bool restore_prev_call_frame();

//fun add(a, b) {
//  var c = a + b;
//  return c;
//}
TEST(x64_jit_compiler_simple_function) {
    struct function_object * function = to_function(
            OP_GET_LOCAL, 0, OP_GET_LOCAL, 1, OP_ADD, OP_SET_LOCAL, 2, OP_POP,
            OP_GET_LOCAL, 2, OP_RETURN,
            OP_EOF
    );
    function->n_arguments = 2;

    struct jit_compilation_result result = jit_compile_arch(function);

    ASSERT_U8_SEQ(result.compiled_code.values,
                  PROLOGUE,
                  SETUP_VM_TO_JIT_MODE_WITH_ARGS(2),
                  0x4c, 0x8b, 0x7d, 0x00, //mov r15, [rbp + 0x0] (OP_GET_LOCAL, 0)
                  0x4c, 0x8b, 0x75, 0x08, //mov r14, [rbp + 0x8] (OP_GET_LOCAL, 1)
                  0x4d, 0x01, 0xf7, //add  r15, r14 (OP_ADD)
                  0x4c, 0x89, 0x7d, 0x10, //mov [rbp + 0x10], r15 (OP_SET_LOCAL, 2)
                  0x4c, 0x8b, 0x7d, 0x10, //mov r15, [rbp + 0x10] (OP_GET_LOCAL, 2)
                  SWITCH_JIT_TO_NATIVE_MODE, //OP_RETURN call to restore_prev_call_frame
                  0x41, 0x51, //push r9
                  0x49, 0xb9, QWORD((uint64_t) &restore_prev_call_frame), //mov r9, restore_prev_call_frame
                  0x41, 0xff, 0xd1, //call r9
                  0x41, 0x59, //pop r9
                  SWITCH_NATIVE_TO_JIT_MODE,
                  0x48, 0x89, 0xec, //mov rsp, rbp
                  0x4c, 0x89, 0x7c, 0x24, 0x00, //mov [rsp + 0x0], r15
                  0x48, 0x83, 0xc4, 0x08, // add rsp, 0x8
                  0x48, 0x89, 0xa3, 0x20, 0x0a, 0x00, 0x00, // mov [rbx + 0xa20], rsp
                  0x48, 0x89, 0xcc, //mov rsp, rcx
                  0x48, 0x89, 0xd5, //mov rbp, rdx
                  EPILOGUE

    );
}

//for(var i = 0; i < 5; i = i + 1) {
//  j = i;
//}
TEST(x64_jit_compiler_for_loop) {
    struct function_object * function = to_function(
            OP_FAST_CONST_8, 0, OP_SET_LOCAL, 0, OP_POP, //i = 0
            OP_GET_LOCAL, 0, OP_FAST_CONST_8, 5, OP_LESS, OP_JUMP_IF_FALSE, 0, 15, //i < 5 jump to OP_NO_OP (note we start counting from OP_GET_LOCAL)
            OP_GET_LOCAL, 0, OP_SET_LOCAL, 1, OP_POP,  //var = i;
            OP_GET_LOCAL, 0, OP_CONST_1, OP_ADD, OP_SET_LOCAL, 0, OP_POP, //i++
            OP_LOOP, 0, 23, //Jump to i < 5 OP_GET_LOCAL (2º line) (we start counting from OP_NO_OP)
            OP_EOF
    );
    function->n_arguments = 2; //Adjust rsp to the number of locals

    uint64_t safepoint_ptr = (uint64_t) &check_gc_on_safe_point_alg;

    struct jit_compilation_result result = jit_compile_arch(function);

    ASSERT_U8_SEQ(result.compiled_code.values,
                  PROLOGUE,
                  SETUP_VM_TO_JIT_MODE_WITH_ARGS(2),
                  0x49, 0xc7, 0xc7, 0x00, 0x00, 0x00, 0x00, //mov r15, 0 (OP_FAST_CONST_8, 0)
                  0x4c, 0x89, 0x7d, 0x00, //mov [rbp+0x0], r15 (OP_SET_LOCAL, 0)
                  0x4c, 0x8b, 0x7d, 0x00, //mov r15, [rbp+0x0] (OP_GET_LOCAL, 0)
                  0x49, 0xc7, 0xc6, 0x05, 0x00, 0x00, 0x00, //mov r14, 0x05 (OP_FAST_CONST_8, 5) 29
                  0x4d, 0x39, 0xfe, //cmp r14, r15 OP_LESS
                  0x0f, 0x9c, 0xc0, //sete al
                  0x4c, 0x0f, 0xb6, 0xf8, //movzx r15, al
                  0x49, 0x83, 0xc7, 0x02, //add r15, 0x2
                  0x49, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, //movabs r14, TRUE_VALUE
                  0x4d, 0x09, 0xf7, //or r15, r14 56
                  0x4c, 0x8b, 0xb3, 0x20, 0x0a, 0x00, 0x00, //mov r14, [rbx + 0xa20] OP_JUMP_IF_FALSE (call to safepoint) load vm_thred esp into r14
                  0x4d, 0x89, 0x7e, 0x00, //mov [r14 + 0x0], r15 (save r15 into esp)
                  0x49, 0x83, 0xc6, 0x08, //add r14, 0x8 Incresae esp pointer by 8
                  0x4c, 0x89, 0xb3, 0x20, 0x0a, 0x00, 0x00, //mov [rbx + 0xa20], r14 Store back vm_thread esp
                  SWITCH_JIT_TO_NATIVE_MODE,
                  0x41, 0x51, //push r9
                  0x49, 0xb9, QWORD(safepoint_ptr), //mov r9, safepoint address
                  0x41, 0xff, 0xd1, //call r9
                  0x41, 0x59, //pop r9
                  SWITCH_NATIVE_TO_JIT_MODE,
                  0x49, 0xbe, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, //mov r14, TRUE_VALUE
                  0x4d, 0x39, 0xfe, //cmp r14, r15
                  0x0f, 0x85, 0x1f, 0x00, 0x00, 0x00, //jne 0xcc (jump to pop rbp)
                  0x4c, 0x8b, 0x7d, 0x00, //mov r15, [rbp + 0x0] (OP_GET_LOCAL, 0)
                  0x4c, 0x89, 0x7d, 0x08, //mov [rbp + 0x8], r15 (OP_SET_LOCAL, 1)
                  0x4c, 0x8b, 0x7d, 0x00, //mov r15, [rbp + 0x0] (OP_GET_LOCAL, 0)
                  0x49, 0xc7, 0xc6, 0x01, 0x00, 0x00, 0x00, //mov r14, 1 (OP_CONST_1)
                  0x4d, 0x01, 0xf7, //add r15, r14 OP_ADD
                  0x4c, 0x89, 0x7d, 0x00,  //mov [rbp + 0x0], r15
                  0xe9, 0x4f, 0xff, 0xff, 0xff, //jmp 0x2a
                  EPILOGUE
    );
}

TEST(x64_jit_compiler_division_multiplication){
    //(1 * 2) / 1
    struct jit_compilation_result result = jit_compile_arch(to_function(OP_CONST_1, OP_CONST_2, OP_MUL, OP_CONST_1, OP_DIV, OP_EOF));

    ASSERT_U8_SEQ(result.compiled_code.values,
                  PROLOGUE,
                  SETUP_VM_TO_JIT_MODE,
                  0x49, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, //mov r15, 1
                  0x49, 0xc7, 0xc6, 0x02, 0x00, 0x00, 0x00, //mov r14, 2
                  0x4d, 0x0f, 0xaf, 0xfe, // imul r15, r14
                  0x49, 0xc7, 0xc6, 0x01, 0x00, 0x00, 0x00, //mov r14, 1
                  0x4c, 0x89, 0xf8, //mov rax, r15
                  0x49, 0xf7, 0xfe, //idiv r14
                  0x49, 0x89, 0xc7, //mov r15,rax
                  EPILOGUE
    );
}

TEST(x64_jit_compiler_negation) {
    struct jit_compilation_result result = jit_compile_arch(to_function(OP_CONST_1, OP_CONST_2, OP_EQUAL,
            OP_NEGATE, OP_NOT, OP_EOF));

    ASSERT_U8_SEQ(result.compiled_code.values,
                  PROLOGUE,
                  SETUP_VM_TO_JIT_MODE,
                  0x49, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, //mov r15, 1
                  0x49, 0xc7, 0xc6, 0x02, 0x00, 0x00, 0x00, //mov r14, 2
                  0x4d, 0x39, 0xfe, //cmp r14, r15 OP_EQUAL
                  0x0f, 0x94, 0xc0, //sete al
                  0x4c, 0x0f, 0xb6, 0xf8, //movzx r15, al
                  0x49, 0x83, 0xc7, 0x02, //add r15, 0x2
                  0x49, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, //movabs r14,0x7ffc000000000000
                  0x4d, 0x09, 0xf7, //or r15,r14
                  0x49, 0xf7, 0xdf, //neg r15 OP_NEGATE
                  0x49, 0xbe, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, //movabs r14,0x7ffc000000000003 OP_NOT
                  0x4d, 0x29, 0xf7, //sub r15, r14
                  0x49, 0xbe, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, //movabs r14,0x7ffc000000000002
                  0x4d, 0x01, 0xf7, //add r15, r14
                  EPILOGUE
    );
}

// 1 + 2 - 3
TEST(x64_jit_compiler_simple_expression) {
    struct jit_compilation_result result = jit_compile_arch(to_function(OP_CONST_1, OP_CONST_2, OP_ADD,
            OP_FAST_CONST_8, 3, OP_SUB, OP_PRINT, OP_EOF));

    uint64_t print_ptr = (uint64_t) &print_lox_value;

    ASSERT_U8_SEQ(result.compiled_code.values,
                  PROLOGUE,
                  SETUP_VM_TO_JIT_MODE,
                  0x49, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, // mov r15, 1
                  0x49, 0xc7, 0xc6, 0x02, 0x00, 0x00, 0x00, // mov r14, 2
                  0x4d, 0x01, 0xf7, // add r15, r14
                  0x49, 0xc7, 0xc6, 0x03, 0x00, 0x00, 0x00, // mov r14, 3
                  0x4d, 0x29, 0xf7, // sub r15, r14
                  SWITCH_JIT_TO_NATIVE_MODE,
                  0x41, 0x51, //push r9
                  0x49, 0xb9, QWORD(print_ptr), //movabs r9, <print function address>
                  0x57, //push rdi
                  0x4c, 0x89, 0xff, //mov rdi, r15
                  0x41, 0xff, 0xd1, //call r9
                  0x5f, //pop rdi
                  0x41, 0x59, //pop r9
                  SWITCH_NATIVE_TO_JIT_MODE,
                  EPILOGUE
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

//Only used for manual inspection
static void x64_jit_compiler_structs_get() {
    struct struct_instance_object * instance = alloc_struct_instance_object();
    struct string_object * field_name = alloc_string_object("x");

    struct function_object * function = to_function(OP_CONSTANT, 0, OP_GET_STRUCT_FIELD, 1, OP_EOF);
    add_constant_to_chunk(&function->chunk, TO_LOX_VALUE_OBJECT(instance)); //Constant 0
    add_constant_to_chunk(&function->chunk, TO_LOX_VALUE_OBJECT(field_name)); //Constant 1

    struct jit_compilation_result result = jit_compile_arch(function);

    print_jit_result(result);
}

//Only used for manual inspection
static void x64_jit_compiler_structs_set() {
    struct struct_instance_object * instance = alloc_struct_instance_object();
    struct string_object * field_name = alloc_string_object("x");

    struct function_object * function = to_function(OP_CONSTANT, 0, OP_CONST_1, OP_SET_STRUCT_FIELD, 1, OP_EOF);
    add_constant_to_chunk(&function->chunk, TO_LOX_VALUE_OBJECT(instance)); //Constant 0
    add_constant_to_chunk(&function->chunk, TO_LOX_VALUE_OBJECT(field_name)); //Constant 1

    struct jit_compilation_result result = jit_compile_arch(function);

    print_jit_result(result);
}

//Only used for manual inspection
static void x64_jit_compiler_structs_initialize() {
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

    struct jit_compilation_result result = jit_compile_arch(function);

    print_jit_result(result);
}