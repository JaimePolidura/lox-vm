#pragma once

#include "test.h"
#include "../src/compiler/compiler.h"
#include "../src/chunk/chunk_disassembler.h"

TEST(simple_compiler_test_with_scope_variables) {
    struct compilation_result result = compile("var edad = 10;\n{\nvar nombre = \"jaime\";\nnombre = 1;\nprint nombre;}");
    struct chunk * chunk = result.chunk;

    ASSERT_TRUE(result.success);

    ASSERT_EQ(chunk->code[0], OP_CONSTANT); //10
    ASSERT_EQ(chunk->code[1], 1); //Constant offset: 10
    ASSERT_EQ(chunk->code[2], OP_DEFINE_GLOBAL);
    ASSERT_EQ(chunk->code[3], 0); //Constant offset: "edad"

    ASSERT_EQ(chunk->code[4], OP_CONSTANT); // "jaime"
    ASSERT_EQ(chunk->code[5], 2); // "jaime"

    ASSERT_EQ(chunk->code[6], OP_CONSTANT); // 1
    ASSERT_EQ(chunk->code[7], 3); // 1
    
    ASSERT_EQ(chunk->code[8], OP_SET_LOCAL)
    ASSERT_EQ(chunk->code[9], 0); //local identifier for nombre

    ASSERT_EQ(chunk->code[10], OP_GET_LOCAL)
    ASSERT_EQ(chunk->code[11], 0); //local identifier for nombre

    ASSERT_EQ(chunk->code[12], OP_PRINT); //local identifier for nombre

    ASSERT_EQ(chunk->code[13], OP_POP); //end of scope
}

TEST(simple_compiler_test) {
    struct compilation_result result = compile("var nombre = \"jaime\";\nnombre = 1 + 2 * 3;\n print nombre;");
    ASSERT_TRUE(result.success);
    struct chunk * chunk = result.chunk;

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
}