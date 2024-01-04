#include "compiler.h"

struct parser {
    struct token current;
    struct token previous;
    bool has_error;
};

struct compiler {
    struct scanner scanner;
    struct parser parser;
    struct chunk chunk;
};

//Lowest to highest
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM, // + -
    PREC_FACTOR, // * /
    PREC_UNARY, // ! -
    PREC_CALL, // . ()
    PREC_PRIMARY
} precedence_t;

typedef void(* parse_fn_t)(struct compiler *);

struct parse_rule {
    parse_fn_t prefix;
    parse_fn_t infix;
    precedence_t precedence;
};

static void report_error(struct compiler * compiler, struct token token, const char * message);
static void advance(struct compiler * compiler);
static void init_parser(struct parser * parser);
static void consume(struct compiler * compiler, tokenType_t expected_token_type, const char * error_message);
static void emit_bytecode(struct compiler * compiler, uint8_t bytecode);
static void emit_bytecodes(struct compiler * compiler, uint8_t bytecodeA, uint8_t bytecodeB);
static void emit_constant(struct compiler * compiler, lox_value_t value);
static struct compiler * alloc_compiler();
static void expression(struct compiler * compiler);
static void number(struct compiler * compiler);
static void grouping(struct compiler * compiler);
static void unary(struct compiler * compiler);
static void parse_precedence(struct compiler * compiler, precedence_t precedence);
static struct parse_rule* get_rule(tokenType_t type);
static void binary(struct compiler * compiler);
static void literal(struct compiler * compiler);
static void string(struct compiler * compiler);

struct parse_rule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
    };

bool compile(char * source_code, struct chunk * output_chunk) {
    struct compiler * compiler = alloc_compiler();

    advance(compiler);
    expression(compiler);
    consume(compiler, TOKEN_EOF, "Expected EOF");

    emit_bytecode(compiler, OP_RETURN);

    bool is_success = !compiler->parser.has_error;
    free(compiler);

    return is_success;
}

static void binary(struct compiler * compiler) {
    tokenType_t token_type = compiler->parser.previous.type;

    struct parse_rule * rule = get_rule(token_type);
    parse_precedence(compiler, rule->precedence + 1);

    switch (token_type) {
        case TOKEN_PLUS: emit_bytecode(compiler, OP_ADD); break;
        case TOKEN_MINUS: emit_bytecode(compiler, OP_SUB); break;
        case TOKEN_STAR: emit_bytecode(compiler, OP_MUL); break;
        case TOKEN_SLASH: emit_bytecode(compiler, OP_DIV); break;
        case TOKEN_BANG_EQUAL: emit_bytecodes(compiler, OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emit_bytecode(compiler, OP_EQUAL); break;
        case TOKEN_GREATER: emit_bytecode(compiler, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_bytecodes(compiler, OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emit_bytecode(compiler, OP_LESS); break;
        case TOKEN_LESS_EQUAL: emit_bytecodes(compiler, OP_GREATER, OP_NOT); break;
    }
}

static struct parse_rule* get_rule(tokenType_t type) {
    return &rules[type];
}

static void string(struct compiler * compiler) {
    const char * string_ptr = compiler->parser.previous.start + 1;
    int string_length = compiler->parser.previous.length - 2;

    struct string_pool_add_result add_result = add_string_pool(&compiler->chunk.compiled_string_pool, string_ptr,
            string_length);
    emit_constant(compiler, FROM_OBJECT(add_result.string_object));
}

static void grouping(struct compiler * compiler) {
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')'");
}

static void literal(struct compiler * compiler) {
    switch (compiler->parser.previous.type) {
        case TOKEN_FALSE: emit_bytecode(compiler, OP_FALSE); break;
        case TOKEN_TRUE: emit_bytecode(compiler, OP_TRUE); break;
        case TOKEN_NIL: emit_bytecode(compiler, OP_NIL); break;;
    }
}

static void unary(struct compiler * compiler) {
    tokenType_t token_type = compiler->parser.previous.type;
    parse_precedence(compiler, PREC_UNARY);

    if(token_type == TOKEN_MINUS) {
        emit_bytecode(compiler, OP_NEGATE);
    }
    if(token_type == TOKEN_BANG) {
        emit_bytecode(compiler, OP_NOT);
    }
}

static void expression(struct compiler * compiler) {
    parse_precedence(compiler, PREC_ASSIGNMENT);
}

static void parse_precedence(struct compiler * compiler, precedence_t precedence) {
    advance(compiler);
    parse_fn_t parse_prefix_fn = get_rule(compiler->parser.previous.type)->prefix;
    if(parse_prefix_fn == NULL) {
        report_error(compiler, compiler->parser.previous, "Expect expression");
    }
    parse_prefix_fn(compiler);

    while(precedence <= get_rule(compiler->parser.current.type)->precedence) {
        advance(compiler);
        parse_fn_t parse_infix_fn = get_rule(compiler->parser.previous.type)->prefix;
        parse_infix_fn(compiler);
    }
}

static void number(struct compiler * compiler) {
    double value = strtod(compiler->parser.previous.start, NULL);
    emit_constant(compiler, FROM_NUMBER(value));
}

static void emit_constant(struct compiler * compiler, lox_value_t value) {
    //TODO Perform contant overflow
    int constant_offset = add_constant_to_chunk(&compiler->chunk, value);
    write_chunk(&compiler->chunk, OP_CONSTANT, compiler->parser.previous.line);
    write_chunk(&compiler->chunk, constant_offset, compiler->parser.previous.line);
}

static void advance(struct compiler * compiler) {
    compiler->parser.previous = compiler->parser.current;

    struct token token = next_token_scanner(&compiler->scanner);
    compiler->parser.current = token;

    if(token.type == TOKEN_ERROR) {
        report_error(compiler, token, "");
    }
}

static void emit_bytecodes(struct compiler * compiler, uint8_t bytecodeA, uint8_t bytecodeB) {
    write_chunk(&compiler->chunk, bytecodeA, compiler->parser.previous.line);
    write_chunk(&compiler->chunk, bytecodeB, compiler->parser.previous.line);
}

static void emit_bytecode(struct compiler * compiler, uint8_t bytecode) {
    write_chunk(&compiler->chunk, bytecode, compiler->parser.previous.line);
}

static void consume(struct compiler * compiler, tokenType_t expected_token_type, const char * error_message) {
    if(compiler->parser.current.type == expected_token_type) {
        advance(compiler);
        return;
    }

    report_error(compiler, compiler->parser.current, error_message);
}

static void report_error(struct compiler * compiler, struct token token, const char * message) {
    fprintf(stderr, "[line %d] Error", token.line);
    if (token.type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token.type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\n", message);
    compiler->parser.has_error = true;
}

static void init_parser(struct parser * parser) {
    parser->has_error = false;
}

static struct compiler * alloc_compiler() {
    struct compiler * compiler = malloc(sizeof(struct compiler));
    init_scanner(&compiler->scanner);
    init_parser(&compiler->parser);
    init_chunk(&compiler->chunk);
    return compiler;
}