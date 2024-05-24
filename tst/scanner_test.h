#pragma once

#include "test.h"
#include "compiler/bytecode/scanner.h"

TEST(simple_scanner_test) {
    auto scanner = alloc_scanner("var nombre = \"jaime\";\nnombre = 1 + 2;\n print nombre;");

    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_VAR);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_IDENTIFIER);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_EQUAL);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_STRING);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_SEMICOLON);

    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_IDENTIFIER);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_EQUAL);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_NUMBER);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_PLUS);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_NUMBER);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_SEMICOLON);

    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_PRINT);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_IDENTIFIER);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_SEMICOLON);
    ASSERT_EQ(next_token_scanner(scanner).type, TOKEN_EOF);
}