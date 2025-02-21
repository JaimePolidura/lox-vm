#include "scanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool at_the_end(struct scanner * scanner);
static struct token create_token(struct scanner * scanner, tokenType_t type);
static struct token create_error_token(struct scanner * scanner, const char * message);
static char advance(struct scanner * scanner);
static bool match(struct scanner * scanner, char expected);
static void skip_white_space(struct scanner * scanner);
static char peek(struct scanner * scanner);
static char peek_next(struct scanner * scanner);
static struct token string(struct scanner* scanner);
static bool is_digit(char character);
static struct token number(struct scanner* scanner);
static bool is_alpha(char character);
static struct token identifier(struct scanner * scanner);
static tokenType_t identifier_type(struct scanner * scanner);
static tokenType_t check_keyword(struct scanner * scanner, int start, int length, const char * rest, tokenType_t type);

struct token next_token_scanner(struct scanner * scanner) {
    skip_white_space(scanner);

    scanner->start = scanner->current;

    if(at_the_end(scanner)) {
        return create_token(scanner, TOKEN_EOF);
    }

    char character = advance(scanner);

    if(is_digit(character)) {
        return number(scanner);
    }
    if(is_alpha(character)) {
        return identifier(scanner);
    }

    switch (character) {
        case '%': return create_token(scanner, TOKEN_PERCENTAGE);
        case '|': return create_token(scanner, TOKEN_BAR);
        case '&': return create_token(scanner, TOKEN_AMPERSAND);
        case ':': return create_token(scanner, TOKEN_COLON);
        case '(': return create_token(scanner, TOKEN_OPEN_PAREN);
        case ')': return create_token(scanner, TOKEN_CLOSE_PAREN);
        case '{': return create_token(scanner, TOKEN_OPEN_BRACE);
        case '}': return create_token(scanner, TOKEN_CLOSE_BRACE);
        case '[': return create_token(scanner, TOKEN_OPEN_SQUARE);
        case ']': return create_token(scanner, TOKEN_CLOSE_SQUARE);
        case ';': return create_token(scanner, TOKEN_SEMICOLON);
        case ',': return create_token(scanner, TOKEN_COMMA);
        case '.': return create_token(scanner, TOKEN_DOT);
        case '-': return create_token(scanner, TOKEN_MINUS);
        case '+': return create_token(scanner, TOKEN_PLUS);
        case '/': return create_token(scanner, TOKEN_SLASH);
        case '*': return create_token(scanner, TOKEN_STAR);
        case '!': return create_token(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return create_token(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return create_token(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : (match(scanner, '<') ? TOKEN_LEFT_SHIFT : TOKEN_LESS));
        case '>': return create_token(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL : (match(scanner, '>') ? TOKEN_RIGHT_SHIFT : TOKEN_GREATER));
        case '"': return string(scanner);
    }

    return create_error_token(scanner, "Unexpected token");
}

static bool match(struct scanner * scanner, char expected) {
    if(at_the_end(scanner)) {
        return false;
    }
    if(*scanner->current != expected) {
        return false;
    }

    scanner->current++;
    return true;
}

static void skip_white_space(struct scanner * scanner) {
    for(;;) {
        switch (peek(scanner)) {
            case ' ':
            case '\t':
            case '\r':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case '/':
                if(peek_next(scanner) == '/') {
                    while(peek(scanner) != '\n' && !at_the_end(scanner)) {
                        advance(scanner);
                    }
                    break;
                }
            default:
                return;
        }
    }
}

static struct token identifier(struct scanner * scanner) {
    while(is_alpha(peek(scanner)) || is_digit(peek(scanner))) {
        advance(scanner);
    }

    return create_token(scanner, identifier_type(scanner));
}

static tokenType_t identifier_type(struct scanner * scanner) {
    switch (scanner->start[0]) {
        case 'a': return check_keyword(scanner, 1, 2, "nd", TOKEN_AND);
        case 'e': return check_keyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'c': return check_keyword(scanner, 1, 4, "onst", TOKEN_CONST);
        case 'i': {
            if(scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'f': return TOKEN_IF;
                    case 'n': return check_keyword(scanner, 2, 4, "line", TOKEN_INLINE);
                }
            }
        }
        case 'n': return check_keyword(scanner, 1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p': {
            if(scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return check_keyword(scanner, 2, 6, "rallel", TOKEN_PARALLEL);
                    case 'r': return check_keyword(scanner, 2, 3, "int", TOKEN_PRINT);
                    case 'u': return check_keyword(scanner, 2, 1, "b", TOKEN_PUB);
                }
            }
        }
        case 'l': return check_keyword(scanner, 1, 2, "en", TOKEN_LEN);
        case 'r': return check_keyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's':
            if(scanner->current - scanner->start > 1){
                switch (scanner->start[1]) {
                    case 't': return check_keyword(scanner, 2, 4, "ruct", TOKEN_STRUCT);
                    case 'y': return check_keyword(scanner, 2, 2, "nc", TOKEN_SYNC);
                }
            }
            return check_keyword(scanner, 1, 5, "truct", TOKEN_STRUCT);
        case 'v': return check_keyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(scanner, 1, 4, "hile", TOKEN_WHILE);
        case 'u': return check_keyword(scanner, 1, 2, "se", TOKEN_USE);
        case 't': return check_keyword(scanner, 1, 3, "rue", TOKEN_TRUE);
        case 'f':
            if(scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return check_keyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(scanner, 2, 1, "n", TOKEN_FUN);
                }
            }
        default:
            return TOKEN_IDENTIFIER;
    }
}

static tokenType_t check_keyword(struct scanner * scanner, int start, int length, const char * rest, tokenType_t type) {
    if(scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static struct token number(struct scanner* scanner) {
    while(is_digit(peek(scanner))) advance(scanner);

    if(peek(scanner) == '.' && is_digit(peek_next(scanner))) {
        advance(scanner);

        while(is_digit(peek(scanner))) advance(scanner);
    }

    if(at_the_end(scanner)) {
        return create_error_token(scanner, "Unterminated immediate.");
    }

    return create_token(scanner, TOKEN_NUMBER);
}

static bool is_alpha(char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_';
}

static struct token string(struct scanner* scanner) {
    while (peek(scanner) != '"' && !at_the_end(scanner)) {
        if(peek(scanner) == '\n') {
            scanner->line++;
        }

        advance(scanner);
    }

    if(at_the_end(scanner)) {
        return create_error_token(scanner, "Unterminated key.");
    }

    advance(scanner);

    return create_token(scanner, TOKEN_STRING);
}

static char peek_next(struct scanner * scanner) {
    if(at_the_end(scanner)) {
        return '\0';
    }

    return *(scanner->current + 1);
}

static char peek(struct scanner * scanner) {
    return *scanner->current;
}

static char advance(struct scanner * scanner) {
    return *scanner->current++;
}

static struct token create_error_token(struct scanner * scanner, const char * message) {
    return (struct token) {
        .type = TOKEN_ERROR,
        .start = message,
        .length = strlen(message),
        .line = scanner->line,
    };
}

static struct token create_token(struct scanner * scanner, tokenType_t type) {
    return (struct token) {
        .type = type,
        .start = scanner->start,
        .line = scanner->line,
        .length = scanner->current - scanner->start
    };
}

static bool at_the_end(struct scanner * scanner) {
    return *scanner->current == '\0';
}

struct scanner * alloc_scanner(char * source_code) {
    struct scanner * scanner = NATIVE_LOX_MALLOC(sizeof(struct scanner));
    init_scanner(scanner, source_code);

    return scanner;
}

static bool is_digit(char character) {
    return character >= '0' && character <= '9';
}

void init_scanner(struct scanner * scanner, char * source_code) {
    scanner->base_source_code = source_code;
    scanner->current = source_code;
    scanner->start = source_code;
    scanner->line = 0;
}

void free_scanner(struct scanner * scanner) {
    //TODO for some reason this segfaults
//    free(scanner->base_source_code);
}
