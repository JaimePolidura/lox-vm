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
        case '(': return create_token(scanner, TOKEN_LEFT_PAREN);
        case ')': return create_token(scanner, TOKEN_RIGHT_PAREN);
        case '{': return create_token(scanner, TOKEN_LEFT_BRACE);
        case '}': return create_token(scanner, TOKEN_RIGHT_BRACE);
        case ';': return create_token(scanner, TOKEN_SEMICOLON);
        case ',': return create_token(scanner, TOKEN_COMMA);
        case '.': return create_token(scanner, TOKEN_DOT);
        case '-': return create_token(scanner, TOKEN_MINUS);
        case '+': return create_token(scanner, TOKEN_PLUS);
        case '/': return create_token(scanner, TOKEN_SLASH);
        case '*': return create_token(scanner, TOKEN_STAR);
        case '!': return create_token(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return create_token(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return create_token(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return create_token(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
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
        if(at_the_end(scanner)) {
            return;
        }

        switch (peek(scanner)) {
            case ' ':
            case '\t':
            case '\r':
                break;
            case '\n':
                scanner->line++;
                break;
            case '/':
                if(peek_next(scanner) == '/') {
                    scanner->line++;
                    break;
                }
            default:
                return;
        }

        advance(scanner);
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
        case 'c': return check_keyword(scanner, 1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'i': return check_keyword(scanner, 1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(scanner, 1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(scanner, 1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(scanner, 1, 4, "uper", TOKEN_SUPER);
        case 'v': return check_keyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(scanner, 1, 4, "hile", TOKEN_WHILE);
        case 'f':
            if(scanner->current - scanner->start > 1) {
                switch (*(scanner->current + 1)) {
                    case 'a': return check_keyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(scanner, 2, 1, "n", TOKEN_FUN);
                }
            }
        case 't':
            if(scanner->current - scanner->start > 1) {
                switch (*(scanner->current + 1)) {
                    case 'h': return check_keyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(scanner, 2, 2, "ue", TOKEN_TRUE);
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
        return create_error_token(scanner, "Unterminated number.");
    }

    return create_token(scanner, TOKEN_NUMBER);
}

static bool is_alpha(char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_';
}

static struct token string(struct scanner* scanner) {
    while (peek(scanner) != '"' && at_the_end(scanner)) {
        if(peek(scanner) == '\n') {
            scanner->line++;
        }

        advance(scanner);
    }

    if(at_the_end(scanner)) {
        return create_error_token(scanner, "Unterminated string.");
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

struct scanner * alloc_scanner() {
    struct scanner * scanner = malloc(sizeof(struct scanner));
    init_scanner(scanner);

    return scanner;
}

static bool is_digit(char character) {
    return character >= '0' && character <= '9';
}

void init_scanner(struct scanner * scanner) {
    scanner->current = NULL;
    scanner->start = NULL;
    scanner->line = 0;
}