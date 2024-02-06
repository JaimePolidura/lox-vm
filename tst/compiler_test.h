#pragma once

#include "test.h"
#include "compiler/compiler.h"
#include "../src/chunk/chunk_disassembler.h"

extern struct trie_list * compiled_packages;

TEST(simple_compiler_test_with_for) {
    struct compilation_result result = compile_standalone("for(var i = 0; i < 5; i = i + 1) {\n print i;\n }");
    struct chunk * chunk = &result.compiled_package->main_function->chunk;
    ASSERT_TRUE(result.success);
    ASSERT_BYTECODE_SEQ(chunk->code,
                        OP_CONSTANT, 0,
                        OP_SET_LOCAL, 1,
                        OP_GET_LOCAL, 1,
                        OP_CONSTANT, 1,
                        OP_LESS,
                        OP_JUMP_IF_FALSE, 0, 13,
                        OP_GET_LOCAL, 1,
                        OP_PRINT,
                        OP_GET_LOCAL, 1,
                        OP_CONSTANT, 2,
                        OP_ADD,
                        OP_SET_LOCAL, 1,
                        OP_LOOP, 0, 21);

    clear_trie(compiled_packages);
}

TEST(simple_compiler_test_with_structs){
    struct compilation_result result = compile_standalone(
            "struct Persona {\nnombre; edad;\n}\nvar jaime = Persona{\"Jaime\" , 21};\nprint jaime.nombre;\njaime.edad = 12;");
    struct chunk * chunk = &result.compiled_package->main_function->chunk;
    ASSERT_TRUE(result.success);
    ASSERT_BYTECODE_SEQ(chunk->code,
                        OP_CONSTANT, 1,
                        OP_CONSTANT, 2,
                        OP_INITIALIZE_STRUCT, 2,
                        OP_DEFINE_GLOBAL, 0,

                        OP_GET_GLOBAL, 3,
                        OP_GET_STRUCT_FIELD, 0,
                        OP_PRINT,

                        OP_GET_GLOBAL, 4,
                        OP_CONSTANT, 5,
                        OP_SET_STRUCT_FIELD, 1);

    clear_trie(compiled_packages);
}

TEST(simple_compiler_test_with_functions) {
    struct compilation_result result = compile_standalone("fun hola(a, b) {\n return a + b;\n }\n print hola(1, 2);");
    struct chunk * chunk = &result.compiled_package->main_function->chunk;
    ASSERT_TRUE(result.success);

    ASSERT_BYTECODE_SEQ(chunk->code,
                        OP_CONSTANT, 1,
                        OP_DEFINE_GLOBAL, 0,
                        OP_GET_GLOBAL, 2,
                        OP_CONSTANT, 3,
                        OP_CONSTANT, 4,
                        OP_CALL, 2);

    struct function_object * function = (struct function_object *) AS_OBJECT(chunk->constants.values[1]);

    ASSERT_BYTECODE_SEQ(function->chunk.code,
                        OP_GET_LOCAL, 1,
                        OP_GET_LOCAL, 2,
                        OP_ADD,
                        OP_RETURN);

    clear_trie(compiled_packages);
}

TEST(simple_compiler_test_if_while) {
    struct compilation_result result = compile_standalone("while(2 == 3) { \n print 1; \n }");
    struct chunk * chunk = &result.compiled_package->main_function->chunk;
    ASSERT_TRUE(result.success);

    ASSERT_BYTECODE_SEQ(chunk->code,
                        OP_CONSTANT, 0,
                        OP_CONSTANT, 1,
                        OP_EQUAL,
                        OP_JUMP_IF_FALSE, 0, 6,
                        OP_CONSTANT, 2, OP_PRINT,
                        OP_LOOP, 0, 14);

    clear_trie(compiled_packages);
}

TEST(simple_compiler_test_if_statements) {
    struct compilation_result result = compile_standalone("if(2 == 3) {\n print 1;\n }else{\n print 2;\n}");
    ASSERT_TRUE(result.success);
    struct chunk * chunk = &result.compiled_package->main_function->chunk;

    ASSERT_BYTECODE_SEQ(chunk->code,
                        OP_CONSTANT, 0,
                        OP_CONSTANT, 1,
                        OP_EQUAL,
                        OP_JUMP_IF_FALSE, 0, 6, // If
                        OP_CONSTANT, 2,
                        OP_PRINT, //print 1
                        OP_JUMP, 0, 3, //Else
                        OP_CONSTANT, 3,
                        OP_PRINT, //print 2
                        );

    clear_trie(compiled_packages);
}

TEST(simple_compiler_test_with_scope_variables) {
    struct compilation_result result = compile_standalone(
            "var edad = 10;\n{\nvar nombre = \"jaime\";\nnombre = 1;\nprint nombre;}");
    ASSERT_TRUE(result.success);
    struct chunk * chunk = &result.compiled_package->main_function->chunk;

    ASSERT_BYTECODE_SEQ(chunk->code,
                        OP_CONSTANT, 1, //var edad = 10
                        OP_DEFINE_GLOBAL, 0,
                        OP_CONSTANT, 2, //var nombre = "jaime"
                        OP_SET_LOCAL, 1,
                        OP_CONSTANT, 3, //nombre = 1;
                        OP_SET_LOCAL, 1,
                        OP_GET_LOCAL, 1, //print nombre;
                        OP_PRINT,
                        OP_POP);

    clear_trie(compiled_packages);
}

TEST(simple_compiler_test) {
    struct compilation_result result = compile_standalone(
            "var nombre = \"jaime\";\nnombre = 1 + 2 * 3;\n print nombre;");
    ASSERT_TRUE(result.success);
    struct chunk * chunk = &result.compiled_package->main_function->chunk;

    ASSERT_EQ(chunk->code[0], OP_CONSTANT);
    ASSERT_EQ(chunk->code[1], 1); //Constant offset: nombre
    ASSERT_EQ(chunk->code[2], OP_DEFINE_GLOBAL);
    ASSERT_EQ(chunk->code[3], 0); //Constant offset: jaime

    ASSERT_EQ(chunk->code[4], OP_CONSTANT);
    ASSERT_EQ(chunk->code[5], 3);
    ASSERT_EQ(chunk->code[6], OP_CONSTANT);
    ASSERT_EQ(chunk->code[7], 4);
    ASSERT_EQ(chunk->code[8], OP_CONSTANT);
    ASSERT_EQ(chunk->code[9], 5);
    ASSERT_EQ(chunk->code[10], OP_MUL);
    ASSERT_EQ(chunk->code[11], OP_ADD);
    ASSERT_EQ(chunk->code[12], OP_SET_GLOBAL);
    ASSERT_EQ(chunk->code[13], 2);

    ASSERT_EQ(chunk->code[14], OP_GET_GLOBAL);
    ASSERT_EQ(chunk->code[15], 6);
    ASSERT_EQ(chunk->code[16], OP_PRINT);

    clear_trie(compiled_packages);
}