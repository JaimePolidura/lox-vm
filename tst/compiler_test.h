#pragma once

#include "test.h"
#include "../src/compiler/compiler.h"
#include "../src/chunk/chunk_disassembler.h"

TEST(simple_compiler_test) {
    struct compilation_result result = compile("var nombre = \"jaime\";\nnombre = 1 + 2 * 3;\n print nombre;");
    ASSERT_TRUE(result.success);
    struct chunk * chunk = result.chunk;

    ASSERT_EQ(chunk->code[0], OP_CONSTANT);
    ASSERT_EQ(chunk->code[1], 1); //Constant offset: nombre
    ASSERT_EQ(chunk->code[2], OP_DEFINE_GLOBAL);
    ASSERT_EQ(chunk->code[3], 0); //Constant offset: jaime

    ASSERT_EQ(chunk->code[4], OP_CONSTANT);
    ASSERT_EQ(chunk->code[5], 3); //Constant offset: 1
    ASSERT_EQ(chunk->code[6], OP_CONSTANT);
    ASSERT_EQ(chunk->code[7], 4); //Constant offset: 2
    ASSERT_EQ(chunk->code[8], OP_CONSTANT);
    ASSERT_EQ(chunk->code[9], 5); //Constant offset: 2
    ASSERT_EQ(chunk->code[10], OP_MUL);
    ASSERT_EQ(chunk->code[11], OP_ADD);
    ASSERT_EQ(chunk->code[12], OP_SET_GLOBAL);
    ASSERT_EQ(chunk->code[13], 2);

    ASSERT_EQ(chunk->code[14], OP_GET_GLOBAL);
    ASSERT_EQ(chunk->code[15], 6);
    ASSERT_EQ(chunk->code[16], OP_PRINT);
}