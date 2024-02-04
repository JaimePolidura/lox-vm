#pragma once

typedef enum {
    // Single-character tokens.
    TOKEN_OPEN_PAREN, TOKEN_CLOSE_PAREN,
    TOKEN_OPEN_BRACE, TOKEN_CLOSE_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    // One or two character tokens.
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    // Literals.
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // Keywords.
    TOKEN_AND, TOKEN_STRUCT, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
    TOKEN_PRINT, TOKEN_RETURN,
    TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
    TOKEN_ERROR,
    TOKEN_EOF,

    TOKEN_PACKAGE,
    TOKEN_PUB,
    TOKEN_IMPORT,
} tokenType_t;

struct token {
    tokenType_t type;
    const char * start;
    int length;
    int line;
};

struct scanner {
    char * start;
    char * current;
    int line;
};

void init_scanner(struct scanner * scanner, char * source_code);
struct scanner * alloc_scanner(char * source_code);
struct token next_token_scanner(struct scanner * scanner);
char * token_to_string(struct token token);