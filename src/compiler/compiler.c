#include "compiler.h"

struct parser {
    struct token current;
    struct token previous;
    bool has_error;
};

struct local {
    struct token name;
    int depth;
};

struct compiler {
    struct compiler * parent; // Used for functions
    struct scanner scanner;
    struct parser parser;
    struct function_object * function;
    function_type_t function_type;
    struct local locals[UINT8_MAX];
    int local_count;
    int local_depth;
};

//Lowest to highest
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} precedence_t;

typedef void(* parse_fn_t)(struct compiler *, bool);

struct parse_rule {
    parse_fn_t prefix;
    parse_fn_t suffix;
    precedence_t precedence;
};

static void report_error(struct compiler * compiler, struct token token, const char * message);
static void advance(struct compiler * compiler);
static void init_parser(struct parser * parser);
static struct compiler * alloc_compiler(function_type_t function_type);
static void init_compiler(struct compiler * compiler, function_type_t function_type, struct compiler * parent_compiler);
static struct function_object * alloc_function_compiler();
static struct function_object * end_compiler(struct compiler * compiler);
static void consume(struct compiler * compiler, tokenType_t expected_token_type, const char * error_message);
static void emit_bytecode(struct compiler * compiler, uint8_t bytecode);
static void emit_bytecodes(struct compiler * compiler, uint8_t bytecodeA, uint8_t bytecodeB);
static void emit_constant(struct compiler * compiler, lox_value_t value);
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
static uint8_t add_string_constant(struct compiler * compiler, struct token string_token);
static void define_global_variable(struct compiler * compiler, uint8_t global_constant_offset);
static void variable(struct compiler * compiler, bool can_assign);
static void named_variable(struct compiler * compiler, struct token previous, bool can_assign);
static void begin_scope(struct compiler * compiler);
static void end_scope(struct compiler * compiler);
static void block(struct compiler * compiler);
static void add_local_variable(struct compiler * compiler, struct token new_variable_name);
static bool is_variable_already_defined(struct compiler * compiler, struct token new_variable_name);
static bool identifiers_equal(struct token * a, struct token * b);
static int resolve_local_variable(struct compiler * compiler, struct token * name);
static void variable_expression_declaration(struct compiler * compiler);
static void if_statement(struct compiler * compiler);
static int emit_jump(struct compiler * compiler, op_code jump_opcode);
static void patch_jump(struct compiler * compiler, int jump_op_index);
static void and(struct compiler * compiler, bool can_assign);
static void or(struct compiler * compiler, bool can_assign);
static void while_statement(struct compiler * compiler);
static void emit_loop(struct compiler * compiler, int loop_start_index);
static void for_loop(struct compiler * compiler);
static void for_loop_initializer(struct compiler * compiler);
static int for_loop_condition(struct compiler * compiler);
static int for_loop_increment(struct compiler * compiler, int loop_start_index);
static struct chunk * current_chunk(struct compiler * compiler);
static void function_declaration(struct compiler * compiler);
static void function(struct compiler * compiler, function_type_t function_type);
static void function_parameters(struct compiler * function_compiler);
static void function_call(struct compiler * compiler, bool can_assign);
static int function_call_number_arguments(struct compiler * compiler);

struct parse_rule rules[] = {
    [TOKEN_OPEN_PAREN] = {grouping, function_call, PREC_CALL},
    [TOKEN_CLOSE_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_OPEN_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLOSE_BRACE] = {NULL, NULL, PREC_NONE},
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
    [TOKEN_AND] = {NULL, and, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or, PREC_NONE},
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

struct compilation_result compile(char * source_code) {
    struct compiler * compiler = alloc_compiler(TYPE_SCRIPT);
    init_scanner(&compiler->scanner, source_code);

    advance(compiler);

    while(!match(compiler, TOKEN_EOF)){
        declaration(compiler);
    }

    write_chunk(current_chunk(compiler), OP_EOF, 0);

    return (struct compilation_result){
        .function_object = end_compiler(compiler),
        .success = !compiler->parser.has_error,
        .chunk = current_chunk(compiler)
    };
}

static void declaration(struct compiler * compiler) {
    if(match(compiler, TOKEN_VAR)) {
        variable_declaration(compiler);
    } else if(match(compiler, TOKEN_FUN)) {
        function_declaration(compiler);
    } else {
        statement(compiler);
    }
}

static void variable_declaration(struct compiler * compiler) {
    consume(compiler, TOKEN_IDENTIFIER, "Expected variable name.");

    if(compiler->local_depth > 0) { // Local scope
        add_local_variable(compiler, compiler->parser.previous);
        variable_expression_declaration(compiler);
    } else { //Global scope
        int variable_identifier_constant = add_string_constant(compiler, compiler->parser.previous);
        variable_expression_declaration(compiler);
        define_global_variable(compiler, variable_identifier_constant);
    }

    consume(compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
}

static void variable_expression_declaration(struct compiler * compiler) {
    if(match(compiler, TOKEN_EQUAL)){
        expression(compiler);
    } else {
        emit_bytecode(compiler, OP_NIL);
    }
}

static void function_declaration(struct compiler * compiler) {
    int function_name_constant_offset = add_string_constant(compiler, compiler->parser.current);
    function(compiler, TYPE_FUNCTION);
    define_global_variable(compiler, function_name_constant_offset);
}

static void function(struct compiler * compiler, function_type_t function_type) {
    struct compiler function_compiler;
    init_compiler(&function_compiler, TYPE_FUNCTION, compiler);
    begin_scope(&function_compiler);

    consume(&function_compiler, TOKEN_OPEN_PAREN, "Expect '(' after function name.");
    function_parameters(&function_compiler);
    consume(&function_compiler, TOKEN_CLOSE_PAREN, "Expect ')' before function body.");
    consume(&function_compiler, TOKEN_OPEN_BRACE, "Expect '{' after function parenthesis.");

    block(&function_compiler);

    struct function_object * function = end_compiler(&function_compiler);
    int function_constant_offset = add_constant_to_chunk(current_chunk(compiler), FROM_OBJECT(function));
    emit_bytecodes(compiler, OP_CONSTANT, function_constant_offset);
}

static void function_parameters(struct compiler * function_compiler) {
    if(!match(function_compiler, TOKEN_CLOSE_PAREN)){
        do {
            function_compiler->function->arity++;
            consume(function_compiler, TOKEN_IDENTIFIER, "Expect variable name in function arguments.");
            add_local_variable(function_compiler, function_compiler->parser.previous);
        } while (match(function_compiler, TOKEN_COMMA));
    }
}

static void function_call(struct compiler * compiler, bool can_assign) {
    int n_args = function_call_number_arguments(compiler);
    emit_bytecodes(compiler, OP_CALL, n_args);
}

static int function_call_number_arguments(struct compiler * compiler) {
    int n_args = 0;

    if(!match(compiler, TOKEN_CLOSE_PAREN)){
        do{
            expression(compiler);
            n_args++;
        } while (match(compiler, TOKEN_COMMA));
    }

    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after function call");

    return n_args;
}

static void statement(struct compiler * compiler) {
    if(match(compiler, TOKEN_PRINT)) {
        print_statement(compiler);
    } else if (match(compiler, TOKEN_OPEN_BRACE)) {
        begin_scope(compiler);
        block(compiler);
        end_scope(compiler);
    } else if(match(compiler, TOKEN_FOR)){
        for_loop(compiler);
    } else if (match(compiler, TOKEN_WHILE)) {
        while_statement(compiler);
    } else if (match(compiler, TOKEN_IF)) {
        if_statement(compiler);
    } else {
        expression_statement(compiler);
    }
}

static void if_statement(struct compiler * compiler) {
    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after if.");
    expression(compiler);
    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after condition.");

    int then_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);
    emit_bytecode(compiler, OP_POP); //We clear the expression result of the if statement

    statement(compiler);
    patch_jump(compiler, then_jump);
    emit_bytecode(compiler, OP_POP);

    int else_jump = emit_jump(compiler, OP_JUMP);

    if(match(compiler, TOKEN_ELSE)){
        statement(compiler);
    }

    patch_jump(compiler, else_jump);
}

static int emit_jump(struct compiler * compiler, op_code jump_opcode) {
    emit_bytecode(compiler, jump_opcode);
    emit_bytecode(compiler, 0x00);
    emit_bytecode(compiler, 0x00);

    return current_chunk(compiler)->in_use - 2;
}

static void patch_jump(struct compiler * compiler, int jump_op_index) {
    int jump = current_chunk(compiler)->in_use - jump_op_index - 2;

    current_chunk(compiler)->code[jump_op_index] = (jump >> 8) & 0xff;
    current_chunk(compiler)->code[jump_op_index + 1] = jump & 0xff;
}

static void and(struct compiler * compiler, bool can_assign) {
    int end_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);
    emit_bytecode(compiler, OP_POP);

    parse_precedence(compiler, PREC_AND);

    patch_jump(compiler, end_jump);
}

static void or(struct compiler * compiler, bool can_assign) {
    int else_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);
    int then_jump = emit_jump(compiler, OP_JUMP);

    patch_jump(compiler, else_jump);
    emit_bytecode(compiler, OP_POP);

    parse_precedence(compiler, PREC_OR);
    patch_jump(compiler, then_jump);
}

static void block(struct compiler * compiler) {
    while(!check(compiler, TOKEN_CLOSE_BRACE) && !check(compiler, TOKEN_EOF)){
        declaration(compiler);
    }

    consume(compiler, TOKEN_CLOSE_BRACE, "Expected '}' after block.");
}

static void expression_statement(struct compiler * compiler) {
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expected ';' after print.");
}

static void while_statement(struct compiler * compiler) {
    int loop_start_index = current_chunk(compiler)->in_use;

    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after while declaration.");
    expression(compiler);
    consume(compiler, TOKEN_OPEN_PAREN, "Expect '{' after while declaration.");

    int exit_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);
    emit_bytecode(compiler, OP_POP);

    statement(compiler);

    emit_loop(compiler, loop_start_index);

    patch_jump(compiler, exit_jump);
    emit_bytecode(compiler, OP_POP);
}

static void for_loop(struct compiler * compiler) {
    begin_scope(compiler);

    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after for.");
    for_loop_initializer(compiler);

    int loop_start_index = current_chunk(compiler)->in_use;

    int loop_jump_if_false_index = for_loop_condition(compiler);

    loop_start_index = for_loop_increment(compiler, loop_start_index);

    statement(compiler);

    emit_loop(compiler, loop_start_index);

    if(loop_jump_if_false_index != -1) { //Has condition
        patch_jump(compiler, loop_jump_if_false_index);
        emit_bytecode(compiler, OP_POP); //Condition
    }

    end_scope(compiler);
}

static void for_loop_initializer(struct compiler * compiler) {
    if(match(compiler, TOKEN_SEMICOLON)){
        //No initializer
    } else if(match(compiler, TOKEN_VAR)){
        variable_declaration(compiler);
    } else {
        expression_statement(compiler);
    }
}

static int for_loop_condition(struct compiler * compiler) {
    int exit_jump_index = -1;

    if(!match(compiler, TOKEN_SEMICOLON)){
        expression(compiler);
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after loop condition");
        exit_jump_index = emit_jump(compiler, OP_JUMP_IF_FALSE);
        emit_bytecode(compiler, OP_POP);
    }

    return exit_jump_index;
}

static int for_loop_increment(struct compiler * compiler, int loop_start_index) {
    if(!match(compiler, TOKEN_CLOSE_PAREN)) { //Has increment
        int jump_to_loop_body_index = emit_jump(compiler, OP_JUMP);
        expression(compiler);
        emit_bytecode(compiler, OP_POP);

        consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after for loop");

        emit_loop(compiler, loop_start_index);
        patch_jump(compiler, jump_to_loop_body_index);
    }

    return -1;
}

static void emit_loop(struct compiler * compiler, int loop_start_index) {
    emit_bytecode(compiler, OP_LOOP);

    int n_opcodes_to_jump = current_chunk(compiler)->in_use - loop_start_index + 2; // +2 to get rid of op_loop two operands

    emit_bytecode(compiler, (n_opcodes_to_jump >> 8) & 0xff);
    emit_bytecode(compiler, n_opcodes_to_jump & 0xff);
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
    int variable_identifier = resolve_local_variable(compiler, &previous);
    bool is_local = variable_identifier != -1;
    uint8_t get_op = is_local ? OP_GET_LOCAL : OP_GET_GLOBAL;
    uint8_t set_op = is_local ? OP_SET_LOCAL : OP_SET_GLOBAL;
    if(!is_local){ //If is global, variable_identifier will contain constant offset, if not it will contain the local index
        variable_identifier = add_string_constant(compiler, previous);
    }

    if(can_assign && match(compiler, TOKEN_EQUAL)){
        expression(compiler);
        emit_bytecodes(compiler, set_op, (uint8_t) variable_identifier);
    } else {
        emit_bytecodes(compiler, get_op, (uint8_t) variable_identifier);
    }
}

static struct parse_rule* get_rule(tokenType_t type) {
    return &rules[type];
}

static void string(struct compiler * compiler, bool can_assign) {
    const char * string_ptr = compiler->parser.previous.start + 1;
    int string_length = compiler->parser.previous.length - 2;

    struct string_pool_add_result add_result = add_string_pool(&current_chunk(compiler)->compiled_string_pool, string_ptr,
            string_length);
    emit_constant(compiler, FROM_OBJECT(add_result.string_object));
}

static void grouping(struct compiler * compiler, bool can_assign) {
    expression(compiler);
    consume(compiler, TOKEN_CLOSE_PAREN, "Expected ')'");
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
        parse_fn_t parse_suffix_fn = get_rule(compiler->parser.previous.type)->suffix;
        parse_suffix_fn(compiler, canAssign);
    }

    if(canAssign && match(compiler, TOKEN_EQUAL)){
        report_error(compiler, compiler->parser.previous, "Invalid assigment target.");
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

static void number(struct compiler * compiler, bool can_assign) {
    double value = strtod(compiler->parser.previous.start, NULL);
    emit_constant(compiler, FROM_NUMBER(value));
}

static void emit_constant(struct compiler * compiler, lox_value_t value) {
    //TODO Perform contant overflow
    int constant_offset = add_constant_to_chunk(current_chunk(compiler), value);
    write_chunk(current_chunk(compiler), OP_CONSTANT, compiler->parser.previous.line);
    write_chunk(current_chunk(compiler), constant_offset, compiler->parser.previous.line);
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
    write_chunk(current_chunk(compiler), bytecodeA, compiler->parser.previous.line);
    write_chunk(current_chunk(compiler), bytecodeB, compiler->parser.previous.line);
}

static void emit_bytecode(struct compiler * compiler, uint8_t bytecode) {
    write_chunk(current_chunk(compiler), bytecode, compiler->parser.previous.line);
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

static struct compiler * alloc_compiler(function_type_t function_type) {
    struct compiler * compiler = malloc(sizeof(struct compiler));
    init_compiler(compiler, function_type, NULL); //Parent compiler null, this function will be the first one to be called
    return compiler;
}

static void init_compiler(struct compiler * compiler, function_type_t function_type, struct compiler * parent_compiler) {
    init_parser(&compiler->parser);
    compiler->function = NULL;
    compiler->parent = parent_compiler;
    compiler->function_type = function_type;
    compiler->local_count = 0;
    compiler->local_depth = 0;
    compiler->function = alloc_function_compiler();

    if(function_type != TYPE_SCRIPT) {
        compiler->function->name = copy_chars_to_string_object(parent_compiler->parser.previous.start,
                                                               parent_compiler->parser.previous.length);
    }

    struct local * local = &compiler->locals[compiler->local_count++];
    local->name.length = 0;
    local->name.start = "";
    local->depth = 0;
}

static struct function_object * end_compiler(struct compiler * compiler) {
    emit_bytecode(compiler, OP_RETURN);
    return compiler->function;
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

static int resolve_local_variable(struct compiler * compiler, struct token * name) {
    for(int i = compiler->local_count - 1; i >= 0; i--){
        struct local * local = &compiler->locals[i];
        if(identifiers_equal(&local->name, name)){
            return i;
        }
    }

    return -1;
}

static void add_local_variable(struct compiler * compiler, struct token new_variable_name) {
    if(compiler->local_depth == 0){
        return; //We are in a global scope
    }

    if(is_variable_already_defined(compiler, new_variable_name)){
        report_error(compiler, new_variable_name, "variable already defined");
    }

    struct local * local = &compiler->locals[compiler->local_count++];
    local->depth = compiler->local_depth;
    local->name = new_variable_name;
}

static bool is_variable_already_defined(struct compiler * compiler, struct token new_variable_name) {
    for(int i = compiler->local_count - 1; i >= 0; i--){
        struct local * local = &compiler->locals[i];
        if(identifiers_equal(&local->name, &new_variable_name)){
            return true;
        }
    }

    return false;
}

static bool identifiers_equal(struct token * a, struct token * b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static uint8_t add_string_constant(struct compiler * compiler, struct token string_token) {
    const char * variable_name = string_token.start;
    int variable_name_length = string_token.length;

    struct string_pool_add_result result_add = add_string_pool(&current_chunk(compiler)->compiled_string_pool,
                                                               variable_name, variable_name_length);

    return add_constant_to_chunk(current_chunk(compiler), FROM_OBJECT(result_add.string_object));
}

static void define_global_variable(struct compiler * compiler, uint8_t global_constant_offset) {
    if(compiler->local_depth == 0) { // Global scope
        emit_bytecodes(compiler, OP_DEFINE_GLOBAL, global_constant_offset);
    }
}

static void begin_scope(struct compiler * compiler) {
    compiler->local_depth++;
}

static void end_scope(struct compiler * compiler) {
    compiler->local_depth--;

    while(compiler->local_count > 0 && compiler->locals[compiler->local_count - 1].depth > compiler->local_depth){
        emit_bytecode(compiler, OP_POP);
        compiler->local_count--;
    }
}

static struct chunk * current_chunk(struct compiler * compiler) {
    return &compiler->function->chunk;
}