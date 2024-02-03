#include "package_compiler.h"

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

typedef void(* parse_fn_t)(struct package_compiler *, bool);

struct parse_rule {
    parse_fn_t prefix;
    parse_fn_t suffix;
    precedence_t precedence;
};

static void report_error(struct package_compiler * compiler, struct token token, const char * message);
static void advance(struct package_compiler * compiler);
static struct package_compiler * alloc_compiler(function_type_t function_type);
static void free_compiler(struct package_compiler * compiler);
static void init_compiler(struct package_compiler * compiler, function_type_t function_type, struct package_compiler * parent_compiler);
static struct compiled_function * alloc_compiled_function();
static struct compiled_function * end_compiler(struct package_compiler * compiler);
static void consume(struct package_compiler * compiler, tokenType_t expected_token_type, const char * error_message);
static void emit_bytecode(struct package_compiler * compiler, uint8_t bytecode);
static void emit_bytecodes(struct package_compiler * compiler, uint8_t bytecodeA, uint8_t bytecodeB);
static void emit_constant(struct package_compiler * compiler, lox_value_t value);
static void expression(struct package_compiler * compiler);
static void number(struct package_compiler * compiler, bool can_assign);
static void grouping(struct package_compiler * compiler, bool can_assign);
static void unary(struct package_compiler * compiler, bool can_assign);
static void parse_precedence(struct package_compiler * compiler, precedence_t precedence);
static struct parse_rule * get_rule(tokenType_t type);
static void binary(struct package_compiler * compiler, bool can_assign);
static void literal(struct package_compiler * compiler, bool can_assign);
static void string(struct package_compiler * compiler, bool can_assign);
static bool match(struct package_compiler * compiler, tokenType_t type);
static bool check(struct package_compiler * compiler, tokenType_t type);
static void declaration(struct package_compiler * compiler);
static void statement(struct package_compiler * compiler);
static void print_statement(struct package_compiler * compiler);
static void expression_statement(struct package_compiler * compiler);
static void variable_declaration(struct package_compiler * compiler);
static uint8_t add_string_constant(struct package_compiler * compiler, struct token string_token);
static void define_global_variable(struct package_compiler * compiler, uint8_t global_constant_offset);
static void variable(struct package_compiler * compiler, bool can_assign);
static void named_variable(struct package_compiler * compiler, struct token variable_name, bool can_assign);
static void begin_scope(struct package_compiler * compiler);
static void end_scope(struct package_compiler * compiler);
static void block(struct package_compiler * compiler);
static int add_local_variable(struct package_compiler * compiler, struct token new_variable_name);
static bool is_variable_already_defined(struct package_compiler * compiler, struct token new_variable_name);
static bool identifiers_equal(struct token * a, struct token * b);
static int resolve_local_variable(struct package_compiler * compiler, struct token * name);
static void variable_expression_declaration(struct package_compiler * compiler);
static void if_statement(struct package_compiler * compiler);
static int emit_jump(struct package_compiler * compiler, op_code jump_opcode);
static void patch_jump_here(struct package_compiler * compiler, int jump_op_index);
static void and(struct package_compiler * compiler, bool can_assign);
static void or(struct package_compiler * compiler, bool can_assign);
static void while_statement(struct package_compiler * compiler);
static void emit_loop(struct package_compiler * compiler, int loop_start_index);
static void for_loop(struct package_compiler * compiler);
static void for_loop_initializer(struct package_compiler * compiler);
static int for_loop_condition(struct package_compiler * compiler);
static void for_loop_increment(struct package_compiler * compiler);
static struct chunk * current_chunk(struct package_compiler * compiler);
static void function_declaration(struct package_compiler * compiler);
static void function(struct package_compiler * compiler, function_type_t function_type);
static void function_parameters(struct package_compiler * function_compiler);
static void function_call(struct package_compiler * compiler, bool can_assign);
static int function_call_number_arguments(struct package_compiler * compiler);
static void return_statement(struct package_compiler * compiler);
static void emit_empty_return(struct package_compiler * compiler);
static void struct_declaration(struct package_compiler * compiler);
static struct struct_definition * register_new_struct(struct package_compiler * compiler, struct token new_struct_name);
static int struct_fields(struct package_compiler * compiler, struct token * fields);
static void free_compiler_structs(struct struct_definition * compiler_structs);
static void struct_initialization(struct package_compiler * compiler);
static struct struct_definition * get_compiler_struct_definition(struct package_compiler * compiler, struct token struct_name);
static int struct_initialization_fields(struct package_compiler * compiler);
static void dot(struct package_compiler * compiler, bool can_assign);
static void add_compilation_struct_instance(struct package_compiler * compiler, struct struct_definition * struct_definition);
static struct struct_instance * find_struct_instance_by_name(struct package_compiler * compiler, struct token name);
static int find_struct_field_offset(struct struct_definition * definition, struct token field_name);
static void package_name(struct package_compiler * compiler);

struct parse_rule rules[] = {
        [TOKEN_OPEN_PAREN] = {grouping, function_call, PREC_CALL},
        [TOKEN_CLOSE_PAREN] = {NULL, NULL, PREC_NONE},
        [TOKEN_OPEN_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_CLOSE_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
        [TOKEN_DOT] = {NULL, dot, PREC_CALL},
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
        [TOKEN_STRUCT] = {NULL, NULL, PREC_NONE},
        [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
        [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
        [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
        [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
        [TOKEN_IF] = {NULL, NULL, PREC_NONE},
        [TOKEN_NIL] = {literal, NULL, PREC_NONE},
        [TOKEN_OR] = {NULL, or, PREC_NONE},
        [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
        [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
        [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
        [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
        [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
        [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
        [TOKEN_EOF] = {NULL, NULL, PREC_NONE},};

struct compilation_result compile(char * source_code) {
    struct package_compiler * compiler = alloc_compiler(TYPE_MAIN_SCOPE);
    init_scanner(compiler->scanner, source_code);

    advance(compiler);

    while(!match(compiler, TOKEN_EOF)){
        declaration(compiler);
    }

    write_chunk(current_chunk(compiler), OP_EOF, 0);

    struct compilation_result compilation_result = {
            .function_object = end_compiler(compiler)->function_object,
            .success = !compiler->parser->has_error,
            .local_count = compiler->local_count,
            .chunk = current_chunk(compiler)
    };

    free_compiler(compiler);

    return compilation_result;
}

static void package_name(struct package_compiler * compiler) {
    if(check(compiler, TOKEN_PACKAGE)){
        consume(compiler, TOKEN_IDENTIFIER, "Expect package name after package");
        compiler->package_name = token_to_string(compiler->parser->previous);
    } else {
        compiler->package_name = "main";
    }
}

static void dot(struct package_compiler * compiler, bool can_assign) {
    consume(compiler, TOKEN_IDENTIFIER, "Expect property name after '.'");

    struct token struct_instance_name = compiler->current_variable_name;
    struct token struct_field_name = compiler->parser->previous;

    struct struct_instance * instance = find_struct_instance_by_name(compiler, struct_instance_name);
    if(instance == NULL){
        report_error(compiler, instance->name, "Cannot find struct instance");
    }
    int struct_field_offset = find_struct_field_offset(instance->struct_definition, struct_field_name);
    if(struct_field_offset == -1) {
        report_error(compiler, struct_field_name, "Unknown field of struct");
    }

    if(can_assign && match(compiler, TOKEN_EQUAL)){
        expression(compiler);
        emit_bytecodes(compiler, OP_SET_STRUCT_FIELD, struct_field_offset);
    } else {
        emit_bytecodes(compiler, OP_GET_STRUCT_FIELD, struct_field_offset);
    }
}

static void declaration(struct package_compiler * compiler) {
    if(match(compiler, TOKEN_VAR)) {
        variable_declaration(compiler);
    } else if(match(compiler, TOKEN_FUN) && compiler->function_type == TYPE_MAIN_SCOPE) {
        function_declaration(compiler);
    } else if(match(compiler, TOKEN_PUB)) {
        consume(compiler, TOKEN_FUN, "Expect fun after pub keyword");

    } else if(match(compiler, TOKEN_STRUCT)) {
        struct_declaration(compiler);
    } else if(match(compiler, TOKEN_FUN) && compiler->function_type == TYPE_FUNCTION_SCOPE){
        report_error(compiler, compiler->parser->current, "Nested functions are not allowed");
    } else {
        statement(compiler);
    }
}

static void struct_declaration(struct package_compiler * compiler) {
    consume(compiler, TOKEN_IDENTIFIER, "Expect struct name");

    struct token struct_name = compiler->parser->previous;
    struct struct_definition * struct_of_declaration = register_new_struct(compiler, struct_name);
    if(struct_of_declaration == NULL){
        report_error(compiler, compiler->parser->previous, "Struct already defined");
    }

    consume(compiler, TOKEN_OPEN_BRACE, "Expect '{' after struct declaration");

    struct token fields [256];
    int n_fields = struct_fields(compiler, fields);

    if(n_fields == 0){
        report_error(compiler, struct_name, "Structs are expected to have at least one field");
    }

    struct_of_declaration->n_fields = n_fields;
    struct_of_declaration->name = struct_name;
    struct_of_declaration->field_names = malloc(sizeof(struct token) * n_fields);
    for(int i = 0; i < n_fields; i++){
        struct_of_declaration->field_names[i] = fields[i];
    }

    struct_of_declaration->next = compiler->structs_definitions;
    compiler->structs_definitions = struct_of_declaration;
}

static int struct_fields(struct package_compiler * compiler, struct token * fields) {
    int current_field_index = 0;

    do {
        consume(compiler, TOKEN_IDENTIFIER, "Expect field in struct declaration");
        fields[current_field_index++] = compiler->parser->previous;
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after struct field");
    }while(!match(compiler, TOKEN_CLOSE_BRACE));

    return current_field_index;
}

static void variable_declaration(struct package_compiler * compiler) {
    consume(compiler, TOKEN_IDENTIFIER, "Expected variable name.");

    compiler->current_variable_name = compiler->parser->previous;

    if(compiler->local_depth > 0) { // Local scope
        int local_variable = add_local_variable(compiler, compiler->parser->previous);
        variable_expression_declaration(compiler);
        emit_bytecodes(compiler, OP_SET_LOCAL, local_variable);
    } else { //Global scope
        int variable_identifier_constant = add_string_constant(compiler, compiler->parser->previous);
        variable_expression_declaration(compiler);
        define_global_variable(compiler, variable_identifier_constant);
    }

    consume(compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
}

static void variable_expression_declaration(struct package_compiler * compiler) {
    if(match(compiler, TOKEN_EQUAL)){
        expression(compiler);
    } else {
        emit_bytecode(compiler, OP_NIL);
    }
}

static void function_declaration(struct package_compiler * compiler) {
    consume(compiler, TOKEN_IDENTIFIER, "Expected compiled_function name after fun keyword.");
    int function_name_constant_offset = add_string_constant(compiler, compiler->parser->previous);
    function(compiler, TYPE_FUNCTION_SCOPE);
    define_global_variable(compiler, function_name_constant_offset);
}

static void function(struct package_compiler * compiler, function_type_t function_type) {
    struct package_compiler function_compiler;
    init_compiler(&function_compiler, TYPE_FUNCTION_SCOPE, compiler);
    begin_scope(&function_compiler);

    consume(&function_compiler, TOKEN_OPEN_PAREN, "Expect '(' after compiled_function name.");
    function_parameters(&function_compiler);
    consume(&function_compiler, TOKEN_OPEN_BRACE, "Expect '{' after compiled_function parenthesis.");

    block(&function_compiler);

    struct compiled_function * function = end_compiler(&function_compiler);

    int function_constant_offset = add_constant_to_chunk(current_chunk(compiler), TO_LOX_VALUE_OBJECT(function->function_object));
    emit_bytecodes(compiler, OP_CONSTANT, function_constant_offset);
}

static void function_parameters(struct package_compiler * function_compiler) {
    if(!check(function_compiler, TOKEN_CLOSE_PAREN)){
        do {
            function_compiler->compiled_function->function_object->n_arguments++;
            consume(function_compiler, TOKEN_IDENTIFIER, "Expect variable name in compiled_function arguments.");
            add_local_variable(function_compiler, function_compiler->parser->previous);
        } while (match(function_compiler, TOKEN_COMMA));
    }

    consume(function_compiler, TOKEN_CLOSE_PAREN, "Expected ')' after compiled_function args");
}

static void function_call(struct package_compiler * compiler, bool can_assign) {
    int n_args = function_call_number_arguments(compiler);
    emit_bytecodes(compiler, OP_CALL, n_args);
}

static int function_call_number_arguments(struct package_compiler * compiler) {
    int n_args = 0;

    if(!check(compiler, TOKEN_CLOSE_PAREN)){
        do{
            expression(compiler);
            n_args++;
        } while (match(compiler, TOKEN_COMMA));
    }

    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after compiled_function call");

    return n_args;
}

static void statement(struct package_compiler * compiler) {
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
    } else if (match(compiler, TOKEN_RETURN)) {
        return_statement(compiler);
    } else {
        expression_statement(compiler);
    }
}

static void return_statement(struct package_compiler * compiler) {
    if(compiler->function_type == TYPE_MAIN_SCOPE){
        report_error(compiler, compiler->parser->previous, "Can't return from top level code");
    }

    if(match(compiler, TOKEN_SEMICOLON)){
        emit_empty_return(compiler);
    } else {
        expression(compiler);
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after return statement");
        emit_bytecode(compiler, OP_RETURN);
    }
}

static void if_statement(struct package_compiler * compiler) {
    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after if.");
    expression(compiler);
    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after condition.");

    int then_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    statement(compiler);

    int else_jump = emit_jump(compiler, OP_JUMP);

    patch_jump_here(compiler, then_jump);

    if(match(compiler, TOKEN_ELSE)){
        statement(compiler);
    }

    patch_jump_here(compiler, else_jump);
}

static int emit_jump(struct package_compiler * compiler, op_code jump_opcode) {
    emit_bytecode(compiler, jump_opcode);
    emit_bytecode(compiler, 0x00);
    emit_bytecode(compiler, 0x00);

    return current_chunk(compiler)->in_use - 2;
}

static void patch_jump_here(struct package_compiler * compiler, int jump_op_index) {
    int jump = current_chunk(compiler)->in_use - jump_op_index - 2;

    current_chunk(compiler)->code[jump_op_index] = (jump >> 8) & 0xff;
    current_chunk(compiler)->code[jump_op_index + 1] = jump & 0xff;
}

static void and(struct package_compiler * compiler, bool can_assign) {
    int end_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    parse_precedence(compiler, PREC_AND);

    patch_jump_here(compiler, end_jump);
}

static void or(struct package_compiler * compiler, bool can_assign) {
    int else_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);
    int then_jump = emit_jump(compiler, OP_JUMP);

    patch_jump_here(compiler, else_jump);

    parse_precedence(compiler, PREC_OR);
    patch_jump_here(compiler, then_jump);
}

static void block(struct package_compiler * compiler) {
    while(!check(compiler, TOKEN_CLOSE_BRACE) && !check(compiler, TOKEN_EOF)){
        declaration(compiler);
    }

    consume(compiler, TOKEN_CLOSE_BRACE, "Expected '}' after block.");
}

static void expression_statement(struct package_compiler * compiler) {
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expected ';'.");
}

static void while_statement(struct package_compiler * compiler) {
    int loop_start_index = current_chunk(compiler)->in_use;

    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after while declaration.");
    expression(compiler);
    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after while declaration.");

    int exit_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    statement(compiler);

    emit_loop(compiler, loop_start_index);

    patch_jump_here(compiler, exit_jump);
}

static void for_loop(struct package_compiler * compiler) {
    begin_scope(compiler);

    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after for.");

    for_loop_initializer(compiler);

    int loop_start_index = current_chunk(compiler)->in_use;

    int loop_jump_if_false_index = for_loop_condition(compiler);

    struct chunk_bytecode_context prev_to_increment_ctx = chunk_start_new_context(&compiler->compiled_function->function_object->chunk);
    for_loop_increment(compiler);
    struct chunk_bytecode_context increment_bytecodes = chunk_restore_context(&compiler->compiled_function->function_object->chunk, prev_to_increment_ctx);

    statement(compiler);

    chunk_write_context(&compiler->compiled_function->function_object->chunk, increment_bytecodes);

    emit_loop(compiler, loop_start_index);

    patch_jump_here(compiler, loop_jump_if_false_index);

    end_scope(compiler);
}

static void for_loop_initializer(struct package_compiler * compiler) {
    if(match(compiler, TOKEN_SEMICOLON)){
        //No initializer
    } else if(match(compiler, TOKEN_VAR)){
        variable_declaration(compiler);
    } else {
        expression_statement(compiler);
    }
}

static int for_loop_condition(struct package_compiler * compiler) {
    expression(compiler);

    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after loop condition");

    return emit_jump(compiler, OP_JUMP_IF_FALSE);
}

static void for_loop_increment(struct package_compiler * compiler) {
    expression(compiler);

    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after for loop");
}

static void emit_loop(struct package_compiler * compiler, int loop_start_index) {
    emit_bytecode(compiler, OP_LOOP);

    int n_opcodes_to_jump = current_chunk(compiler)->in_use - loop_start_index + 2; // +2 to get rid of op_loop two operands

    emit_bytecode(compiler, (n_opcodes_to_jump >> 8) & 0xff);
    emit_bytecode(compiler, n_opcodes_to_jump & 0xff);
}

static void print_statement(struct package_compiler * compiler) {
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expected ';' after print.");
    emit_bytecode(compiler, OP_PRINT);
}

static void variable(struct package_compiler * compiler, bool can_assign) {
    if(check(compiler, TOKEN_OPEN_BRACE)) {
        struct_initialization(compiler);
    } else {
        named_variable(compiler, compiler->parser->previous, can_assign);
    }
}

static void struct_initialization(struct package_compiler * compiler) {
    struct token struct_name = compiler->parser->previous;
    struct struct_definition * struct_definition = get_compiler_struct_definition(compiler, struct_name);
    if(struct_definition == NULL){
        report_error(compiler, struct_name, "Struct not defined");
    }

    advance(compiler); //Consume {

    int n_fields = struct_initialization_fields(compiler);
    consume(compiler, TOKEN_CLOSE_BRACE, "Expect '}' after struct initialization");

    if(n_fields != struct_definition->n_fields){
        report_error(compiler, struct_name, "Struct initialization number of args doest match with definition");
    }

    add_compilation_struct_instance(compiler, struct_definition);

    emit_bytecodes(compiler, OP_INITIALIZE_STRUCT, n_fields);
}

static void add_compilation_struct_instance(struct package_compiler * compiler, struct struct_definition * struct_definition) {
    struct struct_instance * instance = alloc_struct_compilation_instance();
    instance->struct_definition = struct_definition;
    instance->name = compiler->current_variable_name;
    struct struct_instance * prev = compiler->compiled_function->struct_instances;
    instance->next = prev;
    compiler->compiled_function->struct_instances = instance;
}

static int struct_initialization_fields(struct package_compiler * compiler) {
    int n_fields = 0;
    do {
        expression(compiler);
        n_fields++;
    }while(match(compiler, TOKEN_COMMA));

    return n_fields;
}

static void named_variable(struct package_compiler * compiler, struct token variable_name, bool can_assign) {
    compiler->current_variable_name = variable_name;
    int variable_identifier = resolve_local_variable(compiler, &variable_name);
    bool is_local = variable_identifier != -1;
    uint8_t get_op = is_local ? OP_GET_LOCAL : OP_GET_GLOBAL;
    uint8_t set_op = is_local ? OP_SET_LOCAL : OP_SET_GLOBAL;
    if(!is_local){ //If is global, variable_identifier will contain constant offset, if not it will contain the local index
        variable_identifier = add_string_constant(compiler, variable_name);
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

static void string(struct package_compiler * compiler, bool can_assign) {
    const char * string_ptr = compiler->parser->previous.start + 1;
    int string_length = compiler->parser->previous.length - 2;

    struct string_pool_add_result add_result = add_to_global_string_pool(string_ptr, string_length);
    emit_constant(compiler, TO_LOX_VALUE_OBJECT(add_result.string_object));
}

static void grouping(struct package_compiler * compiler, bool can_assign) {
    expression(compiler);
    consume(compiler, TOKEN_CLOSE_PAREN, "Expected ')'");
}

static void literal(struct package_compiler * compiler, bool can_assign) {
    switch (compiler->parser->previous.type) {
        case TOKEN_FALSE: emit_bytecode(compiler, OP_FALSE); break;
        case TOKEN_TRUE: emit_bytecode(compiler, OP_TRUE); break;
        case TOKEN_NIL: emit_bytecode(compiler, OP_NIL); break;
    }
}

static void unary(struct package_compiler * compiler, bool can_assign) {
    tokenType_t token_type = compiler->parser->previous.type;
    parse_precedence(compiler, PREC_UNARY);

    if(token_type == TOKEN_MINUS) {
        emit_bytecode(compiler, OP_NEGATE);
    }
    if(token_type == TOKEN_BANG) {
        emit_bytecode(compiler, OP_NOT);
    }
}

static void expression(struct package_compiler * compiler) {
    parse_precedence(compiler, PREC_ASSIGNMENT);
}

static void parse_precedence(struct package_compiler * compiler, precedence_t precedence) {
    advance(compiler);
    parse_fn_t parse_prefix_fn = get_rule(compiler->parser->previous.type)->prefix;
    if(parse_prefix_fn == NULL) {
        report_error(compiler, compiler->parser->previous, "Expect expression");
    }
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    parse_prefix_fn(compiler, canAssign);

    while(precedence <= get_rule(compiler->parser->current.type)->precedence) {
        advance(compiler);
        parse_fn_t parse_suffix_fn = get_rule(compiler->parser->previous.type)->suffix;
        parse_suffix_fn(compiler, canAssign);
    }

    if(canAssign && match(compiler, TOKEN_EQUAL)){
        report_error(compiler, compiler->parser->previous, "Invalid assigment target.");
    }
}

static void binary(struct package_compiler * compiler, bool can_assign) {
    tokenType_t token_type = compiler->parser->previous.type;

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

static void number(struct package_compiler * compiler, bool can_assign) {
    double value = strtod(compiler->parser->previous.start, NULL);
    emit_constant(compiler, TO_LOX_VALUE_NUMBER(value));
}

static void emit_constant(struct package_compiler * compiler, lox_value_t value) {
    //TODO Perform content overflow check
    int constant_offset = add_constant_to_chunk(current_chunk(compiler), value);
    write_chunk(current_chunk(compiler), OP_CONSTANT, compiler->parser->previous.line);
    write_chunk(current_chunk(compiler), constant_offset, compiler->parser->previous.line);
}

static void advance(struct package_compiler * compiler) {
    compiler->parser->previous = compiler->parser->current;

    struct token token = next_token_scanner(compiler->scanner);
    compiler->parser->current = token;

    if(token.type == TOKEN_ERROR) {
        report_error(compiler, token, "");
    }
}

static void emit_bytecodes(struct package_compiler * compiler, uint8_t bytecodeA, uint8_t bytecodeB) {
    write_chunk(current_chunk(compiler), bytecodeA, compiler->parser->previous.line);
    write_chunk(current_chunk(compiler), bytecodeB, compiler->parser->previous.line);
}

static void emit_bytecode(struct package_compiler * compiler, uint8_t bytecode) {
    write_chunk(current_chunk(compiler), bytecode, compiler->parser->previous.line);
}

static void consume(struct package_compiler * compiler, tokenType_t expected_token_type, const char * error_message) {
    if(compiler->parser->current.type == expected_token_type) {
        advance(compiler);
        return;
    }

    report_error(compiler, compiler->parser->current, error_message);
}

static void report_error(struct package_compiler * compiler, struct token token, const char * message) {
    fprintf(stderr, "[line %d] Error", token.line);
    if (token.type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token.type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\n", message);
    compiler->parser->has_error = true;
}

// Can be freed with free_compiler();
static struct package_compiler * alloc_compiler(function_type_t function_type) {
    struct package_compiler * compiler = malloc(sizeof(struct package_compiler));
    init_compiler(compiler, function_type, NULL); //Parent package_compiler null, this compiled_function will be the first one to be called
    compiler->scanner = malloc(sizeof(struct scanner));
    compiler->parser = malloc(sizeof(struct parser));
    compiler->parser->has_error = false;
    compiler->structs_definitions = NULL;

    return compiler;
}

static void init_compiler(struct package_compiler * compiler, function_type_t function_type, struct package_compiler * parent_compiler) {
    if(parent_compiler != NULL){
        compiler->structs_definitions = parent_compiler->structs_definitions;
        compiler->scanner = parent_compiler->scanner;
        compiler->parser = parent_compiler->parser;
    }

    compiler->parent = parent_compiler;
    compiler->function_type = function_type;
    compiler->local_count = 0;
    compiler->local_depth = 0;
    compiler->compiled_function = alloc_compiled_function();

    if(function_type != TYPE_MAIN_SCOPE) {
        compiler->compiled_function->function_object->name = copy_chars_to_string_object(parent_compiler->parser->previous.start,
                                                                        parent_compiler->parser->previous.length);
    }

    struct local * local = &compiler->locals[compiler->local_count++];
    local->name.length = 0;
    local->name.start = "";
    local->depth = 0;
}

static void free_compiler(struct package_compiler * compiler) {
    free_compiler_structs(compiler->structs_definitions);
    free(compiler->parser);
    free(compiler->scanner);
    free(compiler);
}

static void free_compiler_structs(struct struct_definition * compiler_structs) {
    struct struct_definition * current = compiler_structs;
    while(current != NULL){
        struct struct_definition * prev = current;
        current = prev->next;

        free(prev->field_names);
    }
}

static struct compiled_function * end_compiler(struct package_compiler * compiler) {
    emit_empty_return(compiler);
    return compiler->compiled_function;
}

static bool match(struct package_compiler * compiler, tokenType_t type) {
    if(!check(compiler, type)){
        return false;
    }
    advance(compiler);
    return true;
}

static bool check(struct package_compiler * compiler, tokenType_t type) {
    return compiler->parser->current.type == type;
}

static int resolve_local_variable(struct package_compiler * compiler, struct token * name) {
    for(int i = compiler->local_count - 1; i >= 0; i--){
        struct local * local = &compiler->locals[i];
        if(identifiers_equal(&local->name, name)){
            return i;
        }
    }

    return -1;
}

static int add_local_variable(struct package_compiler * compiler, struct token new_variable_name) {
    if(compiler->local_depth == 0){
        return - 1; //We are in a global scope
    }

    if(is_variable_already_defined(compiler, new_variable_name)){
        report_error(compiler, new_variable_name, "variable already defined");
    }

    struct local * local = &compiler->locals[compiler->local_count++];
    local->depth = compiler->local_depth;
    local->name = new_variable_name;

    return compiler->local_count - 1;
}

static bool is_variable_already_defined(struct package_compiler * compiler, struct token new_variable_name) {
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

static uint8_t add_string_constant(struct package_compiler * compiler, struct token string_token) {
    const char * variable_name = string_token.start;
    int variable_name_length = string_token.length;

    struct string_pool_add_result result_add = add_to_global_string_pool(variable_name, variable_name_length);

    return add_constant_to_chunk(current_chunk(compiler), TO_LOX_VALUE_OBJECT(result_add.string_object));
}

static void define_global_variable(struct package_compiler * compiler, uint8_t global_constant_offset) {
    if(compiler->local_depth == 0) { // Global scope
        emit_bytecodes(compiler, OP_DEFINE_GLOBAL, global_constant_offset);
    }
}

static void begin_scope(struct package_compiler * compiler) {
    compiler->local_depth++;
}

static void end_scope(struct package_compiler * compiler) {
    compiler->local_depth--;

    while(compiler->local_count > 0 && compiler->locals[compiler->local_count - 1].depth > compiler->local_depth){
        emit_bytecode(compiler, OP_POP);
        compiler->local_count--;
    }
}

static struct chunk * current_chunk(struct package_compiler * compiler) {
    return &compiler->compiled_function->function_object->chunk;
}

static void emit_empty_return(struct package_compiler * compiler) {
    emit_bytecodes(compiler, OP_NIL, OP_RETURN);
}

static struct compiled_function * alloc_compiled_function() {
    struct function_object * function_object_ptr = malloc(sizeof(struct function_object));
    function_object_ptr->n_arguments = 0;
    function_object_ptr->name = NULL;
    function_object_ptr->object.type = OBJ_FUNCTION;
    function_object_ptr->object.gc_marked = false;
    init_chunk(&function_object_ptr->chunk);

    struct compiled_function * compiled_function = malloc(sizeof(compiled_function));
    compiled_function->function_object = function_object_ptr;
    compiled_function->struct_instances = NULL;

    return compiled_function;
}

static struct struct_definition * register_new_struct(struct package_compiler * compiler, struct token new_struct_name) {
    struct struct_definition * current = compiler->structs_definitions;
    int new_struct_name_length = new_struct_name.length;
    while(current != NULL){
        if(current->name.length == new_struct_name_length &&
           strncmp(current->name.start, new_struct_name.start, new_struct_name_length) == 0){
            return NULL; //Struct already registered, return null to indicate error
        }
    }

    return alloc_compiler_struct();
}


static struct struct_definition * get_compiler_struct_definition(struct package_compiler * compiler, struct token struct_name) {
    struct struct_definition * current = compiler->structs_definitions;
    int struct_name_length = struct_name.length;
    while(current != NULL){
        if(current->name.length == struct_name_length &&
           strncmp(current->name.start, struct_name.start, struct_name_length) == 0){
            return current; //Struct already registered, return null to indicate error
        }
    }

    return NULL;
}

static struct struct_instance * find_struct_instance_by_name(struct package_compiler * compiler, struct token name) {
    struct struct_instance * current = compiler->compiled_function->struct_instances;
    int struct_name_length = current->name.length;
    while(current != NULL){
        if(current->name.length == name.length &&
           strncmp(current->name.start, name.start, struct_name_length) == 0){
            return current; //Struct already registered, return null to indicate error
        }
    }

    return NULL;
}

static int find_struct_field_offset(struct struct_definition * definition, struct token field_name) {
    for(int i = 0; i < definition->n_fields; i++){
        struct token * current_field = &definition->field_names[i];

        if(current_field->length == field_name.length &&
           strncmp(current_field->start, field_name.start, field_name.length) == 0){
            return i; //Struct already registered, return null to indicate error
        }
    }

    return -1;
}