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

typedef void(* parse_fn_t)(struct compiler *, bool);

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
static void number(struct compiler * compiler, bool can_assign);
static void grouping(struct compiler * compiler, bool can_assign);
static void unary(struct compiler * compiler, bool can_assign);
static void parse_precedence(struct compiler * compiler, precedence_t precedence);
static struct parse_rule * get_rule(tokenType_t type);
static void binary(struct compiler * compiler, bool can_assign);
static void literal(struct compiler * compiler, bool can_assign);
static void string(struct compiler * compiler, bool can_assign);
static bool match(struct compiler * compiler, tokenType_t type);
static bool check(struct compiler * compiler, tokenType_t type);
static void declaration(struct compiler * compiler);
static void statement(struct compiler * compiler);
static void print_statement(struct compiler * compiler);
static void expression_statement(struct compiler * compiler);
static void variable_declaration(struct compiler * compiler);
static uint8_t parse_variable(struct compiler * compiler, char * error_message);
static uint8_t identifier_constant(struct compiler * compiler, struct token identifier_token);
static void define_global_variable(struct compiler * compiler, uint8_t global_constant_offset);
static void variable(struct compiler * compiler, bool can_assign);
static void named_variable(struct compiler * compiler, struct token previous, bool can_assign);

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
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
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

    while(!match(compiler, TOKEN_EOF)){
        declaration(compiler);
    }

    bool is_success = !compiler->parser.has_error;
    free(compiler);

    return is_success;
}

static void declaration(struct compiler * compiler) {
    if(match(compiler, TOKEN_VAR)){
        variable_declaration(compiler);
    } else {
        statement(compiler);
    }
}

static void variable_declaration(struct compiler * compiler) {
    uint8_t constant_offset = parse_variable(compiler, "Expected variable name.");

    if(match(compiler, TOKEN_EQUAL)){
        expression(compiler);
    } else {
        emit_bytecode(compiler, OP_NIL);
    }

    consume(compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

    define_global_variable(compiler, constant_offset);
}

static void statement(struct compiler * compiler) {
    if(match(compiler, TOKEN_PRINT)){
        print_statement(compiler);
    } else {
        expression_statement(compiler);
    }
}

static void expression_statement(struct compiler * compiler) {
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expected ';' after print.");
}

static void print_statement(struct compiler * compiler) {
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expected ';' after print.");
    emit_bytecode(compiler, OP_PRINT);
}

static void variable(struct compiler * compiler, bool can_assign) {
    named_variable(compiler, compiler->parser.previous, can_assign);
}

static void named_variable(struct compiler * compiler, struct token previous, bool can_assign) {
    uint8_t constant_offset = identifier_constant(compiler, previous);
    if(can_assign && match(compiler, TOKEN_EQUAL)){
        expression(compiler);
        emit_bytecode(compiler, OP_SET_GLOBAL);
    } else {
        emit_bytecodes(compiler, OP_GET_GLOBAL, constant_offset);
    }
}

static void binary(struct compiler * compiler, bool can_assign) {
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

static void string(struct compiler * compiler, bool can_assign) {
    const char * string_ptr = compiler->parser.previous.start + 1;
    int string_length = compiler->parser.previous.length - 2;

    struct string_pool_add_result add_result = add_string_pool(&compiler->chunk.compiled_string_pool, string_ptr,
            string_length);
    emit_constant(compiler, FROM_OBJECT(add_result.string_object));
}

static void grouping(struct compiler * compiler, bool can_assign) {
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')'");
}

static void literal(struct compiler * compiler, bool can_assign) {
    switch (compiler->parser.previous.type) {
        case TOKEN_FALSE: emit_bytecode(compiler, OP_FALSE); break;
        case TOKEN_TRUE: emit_bytecode(compiler, OP_TRUE); break;
        case TOKEN_NIL: emit_bytecode(compiler, OP_NIL); break;;
    }
}

static void unary(struct compiler * compiler, bool can_assign) {
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
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    parse_prefix_fn(compiler, canAssign);

    while(precedence <= get_rule(compiler->parser.current.type)->precedence) {
        advance(compiler);
        parse_fn_t parse_infix_fn = get_rule(compiler->parser.previous.type)->prefix;
        parse_infix_fn(compiler, canAssign);
    }

    if(canAssign && match(compiler, TOKEN_EQUAL)){
        report_error(compiler, compiler->parser.previous, "Invalid assigment target.");
    }
}

static void number(struct compiler * compiler, bool can_assign) {
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

static bool match(struct compiler * compiler, tokenType_t type) {
    if(!check(compiler, type)){
        return false;
    }
    advance(compiler);
    return true;
}

static bool check(struct compiler * compiler, tokenType_t type) {
    return compiler->parser.current.type == type;
}

static uint8_t parse_variable(struct compiler * compiler, char * error_message) {
    consume(compiler, TOKEN_IDENTIFIER, error_message);
    return identifier_constant(compiler, compiler->parser.previous);
}

static uint8_t identifier_constant(struct compiler * compiler, struct token identifier_token) {
    const char * variable_name = identifier_token.start;
    int variable_name_length = identifier_token.length;

    struct string_pool_add_result result_add = add_string_pool(&compiler->chunk.compiled_string_pool,
                                                               variable_name, variable_name_length);

    return add_constant_to_chunk(&compiler->chunk, FROM_OBJECT(result_add.string_object));
}

static void define_global_variable(struct compiler * compiler, uint8_t global_constant_offset) {
    emit_bytecodes(compiler, OP_DEFINE_GLOBAL, global_constant_offset);
}
