#include "bytecode_compiler.h"

// Shared between all bytecode_compiler
struct trie_list * compiled_packages = NULL;
const char * compiling_base_dir = NULL;

static void report_error(struct bytecode_compiler * compiler, struct token token, char * message);
static void advance(struct bytecode_compiler * compiler);
static struct bytecode_compiler * alloc_compiler(scope_type_t scope, char * package_name, bool is_standalone_mode);
static void free_compiler(struct bytecode_compiler * compiler);
static void init_compiler(struct bytecode_compiler * compiler, scope_type_t scope_type, struct bytecode_compiler * parent_compiler);
static struct function_object * end_compiler(struct bytecode_compiler * compiler);
static void consume(struct bytecode_compiler * compiler, tokenType_t expected_token_type, char * error_message);
static void emit_bytecode(struct bytecode_compiler * compiler, uint8_t bytecode);
static void emit_bytecodes(struct bytecode_compiler * compiler, uint8_t bytecodeA, uint8_t bytecodeB);
static void emit_u16(struct bytecode_compiler * compiler, uint16_t value);
static void emit_constant(struct bytecode_compiler * compiler, lox_value_t value);
static void emit_package_constant(struct bytecode_compiler * compiler, lox_value_t value);
static void expression(struct bytecode_compiler * compiler);
static void number(struct bytecode_compiler * compiler, bool can_assign);
static void grouping(struct bytecode_compiler * compiler, bool can_assign);
static void unary(struct bytecode_compiler * compiler, bool can_assign);
static struct parse_rule * get_rule(tokenType_t type);
static void binary(struct bytecode_compiler * compiler, bool can_assign);
static void literal(struct bytecode_compiler * compiler, bool can_assign);
static bool match(struct bytecode_compiler * compiler, tokenType_t type);
static bool check(struct bytecode_compiler * compiler, tokenType_t type);
static void declaration(struct bytecode_compiler * compiler);
static void statement(struct bytecode_compiler * compiler);
static void print_statement(struct bytecode_compiler * compiler);
static void expression_statement(struct bytecode_compiler * compiler);
static void var_declaration(struct bytecode_compiler * compiler, bool is_public, bool is_const);
static uint8_t add_string_constant(struct bytecode_compiler * compiler, struct token string_token);
static void define_global_variable(struct bytecode_compiler * compiler, uint8_t global_constant_offset);
static void variable(struct bytecode_compiler * compiler, bool can_assign);
static void named_variable(struct bytecode_compiler * compiler, struct token variable_name, bool can_assign, struct package * external_package, bool is_array);
static void begin_scope(struct bytecode_compiler * compiler);
static void end_scope(struct bytecode_compiler * compiler);
static void block(struct bytecode_compiler * compiler);
static int add_local_variable(struct bytecode_compiler * compiler, struct token new_variable_name, bool *);
static bool is_variable_already_defined(struct bytecode_compiler * compiler, struct token new_variable_name);
static bool identifiers_equal(struct token * a, struct token * b);
static int resolve_local_variable(struct bytecode_compiler * compiler, struct token * name);
static void variable_expression_declaration(struct bytecode_compiler * compiler);
static void if_statement(struct bytecode_compiler * compiler);
static int emit_jump(struct bytecode_compiler * compiler, bytecode_t jump_opcode);
static void patch_jump_here(struct bytecode_compiler * compiler, int jump_op_index);
static void and(struct bytecode_compiler * compiler, bool can_assign);
static void or(struct bytecode_compiler * compiler, bool can_assign);
static void while_statement(struct bytecode_compiler * compiler);
static void emit_loop(struct bytecode_compiler * compiler, int loop_start_index);
static void for_loop(struct bytecode_compiler * compiler);
static void for_loop_initializer(struct bytecode_compiler * compiler);
static int for_loop_condition(struct bytecode_compiler * compiler);
static void for_loop_increment(struct bytecode_compiler * compiler);
static struct chunk * current_chunk(struct bytecode_compiler * compiler);
static void function_declaration(struct bytecode_compiler *compiler, bool is_public, bool is_protected_by_monitor);
static struct function_object * function(struct bytecode_compiler * compiler, bool is_protected_by_monitor);
static void function_parameters(struct bytecode_compiler * function_compiler);
static void function_call(struct bytecode_compiler * compiler, bool can_assign);
static int function_call_number_arguments(struct bytecode_compiler * compiler);
static void return_statement(struct bytecode_compiler * compiler);
static void emit_empty_return(struct bytecode_compiler * compiler);
static void struct_declaration(struct bytecode_compiler * compiler, bool is_public, bool is_const);
static int struct_fields(struct bytecode_compiler * compiler, struct token * fields);
static void struct_initialization(struct bytecode_compiler * compiler, struct package * external_package);
static int struct_initialization_fields(struct bytecode_compiler * compiler);
static void dot(struct bytecode_compiler * compiler, bool can_assign);
static void package_name(struct bytecode_compiler * compiler);
static void add_exported_symbol(struct bytecode_compiler * compiler, struct exported_symbol * exported_symbol, struct token name);
static void import_packages(struct bytecode_compiler * compiler);
static struct bytecode_compiler * start_compiling(char * source_code, char * package_name, bool is_standalone_mode);
static struct package * load_package(struct bytecode_compiler * compiler);
static struct package * compile_package(struct bytecode_compiler * compiler, struct package * package);
static struct package * add_package_to_compiled_packages(char * package_import_name, int package_import_name_length, bool is_standalone_mode);
static struct struct_definition_object * get_struct_definition(struct package * package, struct token name);
static struct exported_symbol * get_exported_symbol(struct bytecode_compiler * compiler, struct package * external_package, struct token variable_name);
static void string(struct bytecode_compiler * compiler, bool can_assign);
static void parallel_declaration(struct bytecode_compiler * compiler);
static void sync_statement(struct bytecode_compiler * compiler);
static void emit_enter_monitor(struct bytecode_compiler * compiler);
static void emit_exit_monitor(struct bytecode_compiler * compiler);
static void emit_exit_all_monitors(struct bytecode_compiler * compiler);
static void array_inline_initialization(struct bytecode_compiler * compiler, bool can_assign);
static int array_inline_initialization_elements(struct bytecode_compiler * compiler);
static void inline_declaration(struct bytecode_compiler * compiler);
static void add_current_function_call(struct bytecode_compiler * bytecode_compiler);
static void add_function_call(struct bytecode_compiler *, struct function_call *);
static void set_compiling_function_name(struct bytecode_compiler *, char *);
static void inline_expression(struct bytecode_compiler *, bool can_assign);
static bool is_const_variable(struct bytecode_compiler *, struct token name, struct package * external_package);
static void array_length(struct bytecode_compiler * compiler, bool can_assign);

typedef void(* parse_fn_t)(struct bytecode_compiler *, bool);

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

struct parse_rule {
    parse_fn_t prefix;
    parse_fn_t suffix;
    precedence_t precedence;
};

static void parse_precedence(struct bytecode_compiler * compiler, precedence_t precedence);

struct parse_rule rules[] = {
        [TOKEN_INLINE] = {inline_expression, NULL, PREC_NONE},
        [TOKEN_OPEN_PAREN] = {grouping, function_call, PREC_CALL},
        [TOKEN_CLOSE_PAREN] = {NULL, NULL, PREC_NONE},
        [TOKEN_OPEN_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_CLOSE_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_OPEN_SQUARE] = {array_inline_initialization, NULL, PREC_NONE},
        [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
        [TOKEN_DOT] = {NULL, dot, PREC_CALL},
        [TOKEN_MINUS] = {unary, binary, PREC_TERM},
        [TOKEN_PERCENTAGE] = {NULL, binary, PREC_TERM},
        [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
        [TOKEN_LEFT_SHIFT] = {NULL, binary, PREC_TERM},
        [TOKEN_RIGHT_SHIFT] = {NULL, binary, PREC_TERM},
        [TOKEN_AMPERSAND] = {NULL, binary, PREC_TERM},
        [TOKEN_BAR] = {NULL, binary, PREC_TERM},
        [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
        [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
        [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
        [TOKEN_BANG] = {unary, NULL, PREC_NONE},
        [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
        [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
        [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
        [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
        [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
        [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
        [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
        [TOKEN_LEN] = {array_length, NULL, PREC_NONE},
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
        [TOKEN_CLOSE_SQUARE] = {NULL, NULL, PREC_NONE},
        [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

struct compilation_result compile_bytecode(char * source_code, char * compiling_package_name, char * base_dir) {
    if(compiled_packages == NULL){
        compiled_packages = alloc_trie_list(NATIVE_LOX_ALLOCATOR());
    }
    if(compiling_base_dir == NULL){
        compiling_base_dir = base_dir;
    }

    struct bytecode_compiler * compiler = start_compiling(source_code, compiling_package_name, false);

    compiler->package->main_function = end_compiler(compiler);
    compiler->package->defined_functions = compiler->defined_functions;
    compiler->package->const_global_variables_names = compiler->const_global_variables;

    struct compilation_result compilation_result = {
            .compiled_package = compiler->package,
            .success = !compiler->parser->has_error,
            .n_compiled_packages = compiler->next_package_id,
            .error_message = NULL,
    };

    free_compiler(compiler);

    return compilation_result;
}

static struct bytecode_compiler * start_compiling(char * source_code, char * package_name, bool is_standalone_mode) {
    struct bytecode_compiler * compiler = alloc_compiler(SCOPE_PACKAGE, package_name, is_standalone_mode);
    init_scanner(compiler->scanner, source_code);

    compiler->package->package_id = compiler->next_package_id++;

    advance(compiler);

    import_packages(compiler);

    while(!match(compiler, TOKEN_EOF)){
        declaration(compiler);
    }

    write_chunk(current_chunk(compiler), OP_EOF);

    return compiler;
}

static void import_packages(struct bytecode_compiler * compiler) {
    while (match(compiler, TOKEN_USE)) {
        consume(compiler, TOKEN_STRING, "Expect import path after use");

        struct token package_import_name = compiler->parser->previous;

        // -2 and +1 to avoid including in the chars "
        add_package_to_compiled_packages(package_import_name.start + 1,
                                         package_import_name.length - 2, compiler->is_standalone_mode);

        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after use");
    }
}

static void dot(struct bytecode_compiler * compiler, bool can_assign) {
    consume(compiler, TOKEN_IDENTIFIER, "Expect property name after '.'");

    struct token struct_field_name = compiler->parser->previous;

    int field_name_string_constant = add_string_constant(compiler, struct_field_name);
    if(can_assign && match(compiler, TOKEN_EQUAL)) {
        expression(compiler);
        emit_bytecodes(compiler, OP_SET_STRUCT_FIELD, field_name_string_constant);
    } else {
        emit_bytecodes(compiler, OP_GET_STRUCT_FIELD, field_name_string_constant);
    }
}

static void declaration(struct bytecode_compiler * compiler) {
    bool is_public = match(compiler, TOKEN_PUB);
    bool is_const = match(compiler, TOKEN_CONST);

    if(match(compiler, TOKEN_VAR)) {
        var_declaration(compiler, is_public, is_const);
    } else if(match(compiler, TOKEN_STRUCT)) {
        struct_declaration(compiler, is_public, is_const);
    } else if(match(compiler, TOKEN_PARALLEL)) {
        parallel_declaration(compiler);
    } else if(match(compiler, TOKEN_INLINE)){
        inline_declaration(compiler);
    } else if(match(compiler, TOKEN_FUN)) {
        bool is_protected_by_monitor = match(compiler, TOKEN_SYNC);
        function_declaration(compiler, is_public, is_protected_by_monitor);
    } else {
        statement(compiler);
    }
}

static void inline_expression(struct bytecode_compiler * compiler, bool can_assign) {
    if(compiler->current_scope == SCOPE_PACKAGE){
        report_error(compiler, compiler->parser->previous, "Cannot inline in top level code");
    }

    compiler->compiling_inline_call = true;
    expression(compiler);
    compiler->compiling_inline_call = false;
}

static void inline_declaration(struct bytecode_compiler * compiler) {
    if(compiler->current_scope == SCOPE_PACKAGE){
        report_error(compiler, compiler->parser->previous, "Cannot inline in top level code");
    }

    compiler->compiling_inline_call = true;
    expression_statement(compiler);
    compiler->compiling_inline_call = false;
}

static void parallel_declaration(struct bytecode_compiler * compiler) {
    compiler->compiling_parallel_call = true;
    expression_statement(compiler);
    compiler->compiling_parallel_call = false;
}

static void struct_declaration(struct bytecode_compiler * compiler, bool is_public, bool is_const) {
    consume(compiler, TOKEN_IDENTIFIER, "Expect struct name");

    struct token struct_name = compiler->parser->previous;
    if(contains_trie(&compiler->package->struct_definitions, struct_name.start, struct_name.length)) {
        report_error(compiler, compiler->parser->previous, "Struct already defined");
    }

    struct struct_definition_object * struct_definition = alloc_struct_definition_object();

    consume(compiler, TOKEN_OPEN_BRACE, "Expect '{' after struct declaration");

    struct token fields [256];
    int n_fields = struct_fields(compiler, fields);

    if(n_fields == 0){
        report_error(compiler, struct_name, "Structs are expected to have at least one field");
    }

    struct_definition->n_fields = n_fields;
    struct_definition->name = add_to_global_string_pool(struct_name.start, struct_name.length, false).string_object;
    struct_definition->field_names = NATIVE_LOX_ALLOCATOR()->lox_malloc(NATIVE_LOX_ALLOCATOR(), sizeof(struct string_object) * n_fields);

    for(int i = 0; i < n_fields; i++) {
        struct token field = fields[i];
        struct_definition->field_names[i] = add_to_global_string_pool(field.start, field.length, false).string_object;
    }

    put_trie(&compiler->package->struct_definitions, struct_definition->name->chars,
             strlen(struct_definition->name->chars), struct_definition);

    if(is_public) {
        uint8_t variable_identifier_constant = add_constant_to_chunk(current_chunk(compiler), TO_LOX_VALUE_OBJECT(struct_definition->name));
        emit_constant(compiler, TO_LOX_VALUE_OBJECT(struct_definition));
        define_global_variable(compiler, variable_identifier_constant);

        add_exported_symbol(compiler, alloc_exported_symbol(variable_identifier_constant, is_const, EXPORTED_STRUCT), struct_name);
    }
}

static int struct_fields(struct bytecode_compiler * compiler, struct token * fields) {
    int current_field_index = 0;

    do {
        consume(compiler, TOKEN_IDENTIFIER, "Expect field in struct declaration");
        fields[current_field_index++] = compiler->parser->previous;
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after struct field");
    }while(!match(compiler, TOKEN_CLOSE_BRACE));

    return current_field_index;
}

static void emtpy_array_initialization(struct bytecode_compiler * compiler) {
    consume(compiler, TOKEN_NUMBER, "Expect array size after '['");
    struct token number_elements_token = compiler->parser->previous;
    consume(compiler, TOKEN_CLOSE_SQUARE, "Expect ']' after array initialization");

    int n_elements = strtod(number_elements_token.start, NULL);

    emit_bytecode(compiler, OP_INITIALIZE_ARRAY);
    emit_u16(compiler, n_elements);
    emit_bytecode(compiler, 1); //Emtpy intialization
}

static void var_declaration(struct bytecode_compiler * compiler, bool is_public, bool is_const) {
    consume(compiler, TOKEN_IDENTIFIER, "Expected variable name.");
    struct token variable_name = compiler->parser->previous;
    bool is_array_empty_initialization = false;

    if (match(compiler, TOKEN_OPEN_SQUARE)) {
        emtpy_array_initialization(compiler);
        is_array_empty_initialization = true;
    }

    bool is_local_variable = compiler->local_depth > 0;

    if(is_public && is_local_variable){
        report_error(compiler, compiler->parser->previous, "Cannot declare local variables value_as public");
    }
    if(is_const && is_local_variable){
        report_error(compiler, compiler->parser->previous, "Cannot declare local variables value_as const");
    }
    if(is_const && !is_local_variable) {
        char * copy_variable_name = copy_string(variable_name.start, variable_name.length);
        put_trie(&compiler->const_global_variables, copy_variable_name, variable_name.length, NON_TRIE_VALUE);
    }

    bool newly_added_local_variable = false;

    int variable_identifier = is_local_variable ?
        add_local_variable(compiler, variable_name, &newly_added_local_variable) :
        add_string_constant(compiler, variable_name);

    if (newly_added_local_variable) {
         compiler->compiling_new_local_set_body = variable_identifier;
    }

    if (!is_array_empty_initialization) {
        variable_expression_declaration(compiler);
    }

    if (newly_added_local_variable) {
        compiler->compiling_new_local_set_body = -2;
    }

    if(is_local_variable) { // Local current_scope
        emit_bytecodes(compiler, OP_SET_LOCAL, variable_identifier);
    } else {
        define_global_variable(compiler, variable_identifier);
    }

    if(is_public) {
        add_exported_symbol(compiler, alloc_exported_symbol(variable_identifier, is_const, EXPORTED_VAR), variable_name);
    }

    consume(compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
}

static void variable_expression_declaration(struct bytecode_compiler * compiler) {
    if(match(compiler, TOKEN_EQUAL)){
        expression(compiler);
    } else {
        emit_bytecode(compiler, OP_NIL);
    }
}

static void function_declaration(struct bytecode_compiler *compiler, bool is_public, bool is_protected_by_monitor) {
    if(compiler->current_scope == SCOPE_FUNCTION){
        report_error(compiler, compiler->parser->current, "Nested functions are not allowed");
    }

    consume(compiler, TOKEN_IDENTIFIER, "Expected current_function name after fun keyword.");
    struct token function_name = compiler->parser->previous;
    int function_name_constant_offset = add_string_constant(compiler, function_name);
    function(compiler, is_protected_by_monitor);
    define_global_variable(compiler, function_name_constant_offset);

    if(is_public){
        add_exported_symbol(compiler, alloc_exported_symbol(function_name_constant_offset, false, EXPORTED_FUNCTION), function_name);
    }
}

static struct function_object * function(struct bytecode_compiler * compiler, bool is_protected_by_monitor) {
    struct bytecode_compiler function_compiler;
    memset(&function_compiler, 0, sizeof(struct bytecode_compiler));
    init_compiler(&function_compiler, SCOPE_FUNCTION, compiler);
    begin_scope(&function_compiler);

    consume(&function_compiler, TOKEN_OPEN_PAREN, "Expect '(' after current_function name.");
    function_parameters(&function_compiler);
    consume(&function_compiler, TOKEN_OPEN_BRACE, "Expect '{' after current_function parenthesis.");

    if(is_protected_by_monitor)
        emit_enter_monitor(&function_compiler);

    block(&function_compiler);

    emit_exit_all_monitors(&function_compiler);

    struct function_object * function = end_compiler(&function_compiler);

    emit_bytecode(&function_compiler, OP_EOF);

    put_hash_table(&compiler->defined_functions, function->name, TO_LOX_VALUE_OBJECT(function));

    uint8_t function_constant_offset = add_constant_to_chunk(current_chunk(compiler), TO_LOX_VALUE_OBJECT(function));
    emit_bytecodes(compiler, OP_CONSTANT, function_constant_offset);

    return function;
}

static void function_parameters(struct bytecode_compiler * function_compiler) {
    if(!check(function_compiler, TOKEN_CLOSE_PAREN)){
        do {
            function_compiler->current_function->n_arguments++;
            consume(function_compiler, TOKEN_IDENTIFIER, "Expect variable name in current_function arguments.");
            add_local_variable(function_compiler, function_compiler->parser->previous, NULL);
        } while (match(function_compiler, TOKEN_COMMA));
    }

    consume(function_compiler, TOKEN_CLOSE_PAREN, "Expected ')' after current_function args");
}

static void function_call(struct bytecode_compiler * compiler, bool can_assign) {
    int n_args = function_call_number_arguments(compiler);

    if (compiler->compiling_extermal_symbol_call) {
        lox_value_t package_lox_type = to_lox_package(compiler->package_of_external_symbol);
        emit_package_constant(compiler, package_lox_type);
        emit_bytecode(compiler, OP_ENTER_PACKAGE);
    }

    emit_bytecodes(compiler, OP_CALL, n_args);
    emit_bytecode(compiler, compiler->compiling_parallel_call ? 1 : 0);

    add_current_function_call(compiler);

    if (compiler->compiling_inline_call) {
        compiler->compiling_inline_call = false;
    }

    if (compiler->compiling_extermal_symbol_call) {
        emit_bytecode(compiler, OP_EXIT_PACKAGE);

        compiler->package_of_external_symbol = NULL;
        compiler->compiling_extermal_symbol_call = false;
    }
}

static int function_call_number_arguments(struct bytecode_compiler * compiler) {
    int n_args = 0;

    if(!check(compiler, TOKEN_CLOSE_PAREN)){
        do{
            expression(compiler);
            n_args++;
        } while (match(compiler, TOKEN_COMMA));
    }

    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after current_function call");

    return n_args;
}

static void statement(struct bytecode_compiler * compiler) {
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
    } else if(match(compiler, TOKEN_SYNC)) {
        sync_statement(compiler);
    } else {
        expression_statement(compiler);
    }
}

static void sync_statement(struct bytecode_compiler * compiler) {
    emit_enter_monitor(compiler);

    statement(compiler);

    emit_exit_monitor(compiler);
}

static void return_statement(struct bytecode_compiler * compiler) {
    if (compiler->current_scope == SCOPE_PACKAGE) {
        report_error(compiler, compiler->parser->previous, "Can't return from top level code");
    }

    if (match(compiler, TOKEN_SEMICOLON)) {
        emit_exit_all_monitors(compiler);
        emit_empty_return(compiler);
    } else {
        expression(compiler);
        emit_exit_all_monitors(compiler);
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after return statement");
        emit_bytecode(compiler, OP_RETURN);
    }
}

static void if_statement(struct bytecode_compiler * compiler) {
    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after if.");
    expression(compiler);
    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after jump_to_operand.");

    int then_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    statement(compiler);

    int else_jump = emit_jump(compiler, OP_JUMP);

    patch_jump_here(compiler, then_jump);

    if(match(compiler, TOKEN_ELSE)){
        statement(compiler);
    }

    patch_jump_here(compiler, else_jump);
}

static int emit_jump(struct bytecode_compiler * compiler, bytecode_t jump_opcode) {
    emit_bytecode(compiler, jump_opcode);
    emit_bytecode(compiler, 0x00);
    emit_bytecode(compiler, 0x00);

    return current_chunk(compiler)->in_use - 2;
}

static void patch_jump_here(struct bytecode_compiler * compiler, int jump_op_index) {
    int jump = current_chunk(compiler)->in_use - jump_op_index - 2;
    write_u16_le(&current_chunk(compiler)->code[jump_op_index], jump);
}

static void and(struct bytecode_compiler * compiler, bool can_assign) {
    int end_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    parse_precedence(compiler, PREC_AND);

    patch_jump_here(compiler, end_jump);
}

static void or(struct bytecode_compiler * compiler, bool can_assign) {
    int else_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);
    int then_jump = emit_jump(compiler, OP_JUMP);

    patch_jump_here(compiler, else_jump);

    parse_precedence(compiler, PREC_OR);
    patch_jump_here(compiler, then_jump);
}

static void block(struct bytecode_compiler * compiler) {
    while(!check(compiler, TOKEN_CLOSE_BRACE) && !check(compiler, TOKEN_EOF)){
        declaration(compiler);
    }

    consume(compiler, TOKEN_CLOSE_BRACE, "Expected '}' after block.");
}

static void expression_statement(struct bytecode_compiler * compiler) {
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expected ';'.");

    if (compiler->compiling_set_operation || compiler->compiling_set_array_element_operation) {
        compiler->compiling_set_array_element_operation = false;
        compiler->compiling_set_operation = false;
    } else {
        emit_bytecode(compiler, OP_POP);
    }
}

static void while_statement(struct bytecode_compiler * compiler) {
    int loop_start_index = current_chunk(compiler)->in_use;

    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after while declaration.");
    expression(compiler);
    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after while declaration.");

    int exit_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    statement(compiler);

    emit_loop(compiler, loop_start_index);

    patch_jump_here(compiler, exit_jump);
}

static void for_loop(struct bytecode_compiler * compiler) {
    begin_scope(compiler);

    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after for.");

    for_loop_initializer(compiler);

    int loop_start_index = current_chunk(compiler)->in_use;

    int loop_jump_if_false_index = for_loop_condition(compiler);

    struct chunk_bytecode_context prev_to_increment_ctx = chunk_start_new_context(compiler->current_function->chunk);
    for_loop_increment(compiler);
    struct chunk_bytecode_context increment_bytecodes = chunk_restore_context(compiler->current_function->chunk,
            prev_to_increment_ctx);

    statement(compiler);

    chunk_write_context(compiler->current_function->chunk, increment_bytecodes);

    emit_loop(compiler, loop_start_index);

    patch_jump_here(compiler, loop_jump_if_false_index);

    end_scope(compiler);
}

static void for_loop_initializer(struct bytecode_compiler * compiler) {
    if(match(compiler, TOKEN_SEMICOLON)){
        //No initializer
    } else if(match(compiler, TOKEN_VAR)){
        var_declaration(compiler, false, false);
    } else {
        expression_statement(compiler);
    }
}

static int for_loop_condition(struct bytecode_compiler * compiler) {
    expression(compiler);

    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after loop jump_to_operand");

    int offset = emit_jump(compiler, OP_JUMP_IF_FALSE);

    return offset;
}

static void for_loop_increment(struct bytecode_compiler * compiler) {
    expression(compiler);
    compiler->compiling_set_operation = false;

    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after for loop");
}

static void emit_loop(struct bytecode_compiler * compiler, int loop_start_index) {
    emit_bytecode(compiler, OP_LOOP);
    int n_opcodes_to_jump = current_chunk(compiler)->in_use - loop_start_index + 2; // +2 to get rid of op_loop two operands

    emit_u16(compiler, n_opcodes_to_jump);
}

static void print_statement(struct bytecode_compiler * compiler) {
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expected ';' after print.");
    emit_bytecode(compiler, OP_PRINT);
}

static void array_length(struct bytecode_compiler * compiler, bool can_assign) {
    consume(compiler, TOKEN_OPEN_PAREN, "Expect '(' after len");
    expression(compiler);
    emit_bytecode(compiler, OP_GET_ARRAY_LENGTH);
    consume(compiler, TOKEN_CLOSE_PAREN, "Expect ')' after len");
}

static void variable(struct bytecode_compiler * compiler, bool can_assign) {
    bool is_from_function = check(compiler, TOKEN_OPEN_PAREN);
    bool is_from_array = check(compiler, TOKEN_OPEN_SQUARE);
    bool is_from_package = check(compiler, TOKEN_COLON);
    struct package * external_package = NULL;
    struct token package_name = compiler->parser->previous;
    struct token variable_name;

    if (is_from_package) {
        external_package = load_package(compiler);
        consume(compiler, TOKEN_COLON, "Expect ':' after : when accessing a package symbol");
        consume(compiler, TOKEN_COLON, "Expect ':' after : when accessing a package symbol");
        advance(compiler); //As we are accessing the variable name with the previous token, we need to advance
        variable_name = compiler->parser->previous;
        is_from_function = check(compiler, TOKEN_OPEN_PAREN);
    } else if(is_from_array) {
        variable_name = compiler->parser->previous;
        advance(compiler); //Consume [
        expression(compiler); //Array index
        consume(compiler, TOKEN_CLOSE_SQUARE, "Expect `]` when accessing array");
    } else {
        variable_name = compiler->parser->previous;
    }

    if(is_from_function && compiler->compiling_parallel_call && compiler->compiling_inline_call) {
        report_error(compiler, compiler->parser->previous, "'inline' and 'parallel' cannot be used together");
    }

    if (check(compiler, TOKEN_OPEN_BRACE)) {
        struct_initialization(compiler, external_package);
    } else {
        named_variable(compiler, variable_name, can_assign, external_package, is_from_array);
    }

    if(is_from_function) {
        int function_name_length = variable_name.length;
        char * function_name = copy_string(variable_name.start, function_name_length);
        compiler->current_function_call_name = function_name;
        put_trie(&compiler->function_call_list, function_name, function_name_length, NON_TRIE_VALUE);
    }
}

static struct package * load_package(struct bytecode_compiler * compiler) {
    struct token package_name = compiler->parser->previous;

    struct package * package = find_trie(compiled_packages, package_name.start, package_name.length);
    if(package == NULL){
        report_error(compiler, package_name, "Cannot find package");
    }

    //Precompiled, may part of some library
    if(package->state == PENDING_INITIALIZATION && package->package_id == 0){
        package->package_id = ++compiler->next_package_id;
    }
    if(package->state == PENDING_COMPILATION){
        compile_package(compiler, package);
    }

    emit_package_constant(compiler, to_lox_package(package));

    return package;
}

static struct package * compile_package(struct bytecode_compiler * compiler, struct package * package) {
    if(compiler->is_standalone_mode){
        report_error(compiler, compiler->parser->previous, "Cannot use local packages in standalone mode");
    }

    char * source_code = read_package_source_code(package->absolute_path);
    struct compilation_result compilation_result = compile_bytecode(source_code, package->name, compiling_base_dir);
    package->state = PENDING_INITIALIZATION;
    package->package_id = ++compiler->next_package_id;

    NATIVE_LOX_FREE(source_code);

    return package;
}

static void struct_initialization(struct bytecode_compiler * compiler, struct package * external_package) {
    struct token struct_name = compiler->parser->previous;
    struct struct_definition_object * struct_definition = external_package != NULL ?
            get_struct_definition(external_package, struct_name) :
            get_struct_definition(compiler->package, struct_name);

    if (struct_definition == NULL) {
        report_error(compiler, struct_name, "Struct not defined");
    }

    advance(compiler); //Consume {

    int n_fields = struct_initialization_fields(compiler);
    consume(compiler, TOKEN_CLOSE_BRACE, "Expect '}' after struct initialization");

    if(n_fields != struct_definition->n_fields){
        report_error(compiler, struct_name, "Struct initialization immediate of args doest match with struct_definition");
    }

    uint8_t struct_definition_constant = add_constant_to_chunk(current_chunk(compiler), TO_LOX_VALUE_OBJECT(struct_definition));

    emit_bytecodes(compiler, OP_INITIALIZE_STRUCT, struct_definition_constant);
}

static int struct_initialization_fields(struct bytecode_compiler * compiler) {
    int n_fields = 0;
    do {
        expression(compiler);
        n_fields++;
    }while(match(compiler, TOKEN_COMMA));

    return n_fields;
}

static void named_variable(
        struct bytecode_compiler * compiler,
        struct token variable_name,
        bool can_assign,
        struct package * external_package,
        bool is_array
) {
    int variable_identifier = resolve_local_variable(compiler, &variable_name);

    if (variable_identifier == compiler->compiling_new_local_set_body) {
        report_error(compiler, variable_name, "Variable is not yet defined");
    }

    bool is_from_external_package = external_package != NULL;
    bool is_const = is_const_variable(compiler, variable_name, external_package);
    bool is_local = variable_identifier != -1;
    bool is_global = !is_local;
    bool is_set_op = can_assign && match(compiler, TOKEN_EQUAL);

    //When setting values of an array we don't use OP_SET_LOCAL/OP_SET_GLOBAL we use OP_SET_ARRAY_ELEMENT
    uint8_t op = is_set_op && !is_array ?
            (is_local ? OP_SET_LOCAL : OP_SET_GLOBAL) :
            (is_local ? OP_GET_LOCAL : OP_GET_GLOBAL);

    if (is_global && !is_from_external_package) {
        variable_identifier = add_string_constant(compiler, variable_name);
    }
    if (is_set_op && is_const) {
        report_error(compiler, variable_name, "Cannot set a value_node to a declared const variable");
    }
    if (is_set_op && !is_array && compiler->compiling_set_operation){
        report_error(compiler, compiler->parser->previous, "Assignments cannot be used value_as expressions");
    }
    if (is_set_op && !is_array && !compiler->compiling_set_operation){
        compiler->compiling_set_operation = true;
    }
    if(is_set_op && is_array && !compiler->compiling_set_array_element_operation){
        compiler->compiling_set_array_element_operation = true;
    }

    if (is_global && is_from_external_package) {
        struct exported_symbol * exported_symbol = get_exported_symbol(compiler, external_package, variable_name);
        exported_symbol_type_t expected_exported_type = compiler->parser->current.type == TOKEN_OPEN_PAREN ?
                EXPORTED_FUNCTION : EXPORTED_VAR;

        if(exported_symbol->type != expected_exported_type) {
            report_error(compiler, variable_name, "Exported type is invalid in this context");
        }

        variable_identifier = exported_symbol->constant_identifier;

        compiler->package_of_external_symbol = external_package;
        compiler->compiling_extermal_symbol_call = true;
    }

    if (is_set_op) {
        expression(compiler);
    }

    //The opcode const of the package is added by load_package()
    if (is_from_external_package) {
        emit_bytecode(compiler, OP_ENTER_PACKAGE);
    }

    emit_bytecodes(compiler, op, (uint8_t) variable_identifier);

    if(is_from_external_package) {
        emit_bytecode(compiler, OP_EXIT_PACKAGE);
    }

    if(is_array) {
        //If it is an array, the index expression has been emitted before calling this function
        emit_bytecode(compiler,is_set_op ? OP_SET_ARRAY_ELEMENT : OP_GET_ARRAY_ELEMENT);
    }
}

static struct parse_rule* get_rule(tokenType_t type) {
    return &rules[type];
}

static void grouping(struct bytecode_compiler * compiler, bool can_assign) {
    expression(compiler);
    consume(compiler, TOKEN_CLOSE_PAREN, "Expected ')'");
}

static void literal(struct bytecode_compiler * compiler, bool can_assign) {
    switch (compiler->parser->previous.type) {
        case TOKEN_FALSE: emit_bytecode(compiler, OP_FALSE); break;
        case TOKEN_TRUE: emit_bytecode(compiler, OP_TRUE); break;
        case TOKEN_NIL: emit_bytecode(compiler, OP_NIL); break;
    }
}

static void array_inline_initialization(struct bytecode_compiler * compiler, bool can_assign) {
    uint16_t n_elements = (uint16_t) array_inline_initialization_elements(compiler);

    emit_bytecode(compiler, OP_INITIALIZE_ARRAY);
    emit_u16(compiler, n_elements);
    emit_bytecode(compiler, 0); //Not emtpy intialization
}

static int array_inline_initialization_elements(struct bytecode_compiler * compiler) {
    int n_elements = 0;

    do {
        n_elements++;
        expression(compiler);
    }while(match(compiler, TOKEN_COMMA));

    consume(compiler, TOKEN_CLOSE_SQUARE, "Expect ']' after array initialization");

    return n_elements;
}

static void unary(struct bytecode_compiler * compiler, bool can_assign) {
    tokenType_t token_type = compiler->parser->previous.type;
    parse_precedence(compiler, PREC_UNARY);

    if(token_type == TOKEN_MINUS) {
        emit_bytecode(compiler, OP_NEGATE);
    }
    if(token_type == TOKEN_BANG) {
        emit_bytecode(compiler, OP_NOT);
    }
}

static void expression(struct bytecode_compiler * compiler) {
    parse_precedence(compiler, PREC_ASSIGNMENT);
}

static void parse_precedence(struct bytecode_compiler * compiler, precedence_t precedence) {
    advance(compiler);
    parse_fn_t parse_prefix_fn = get_rule(compiler->parser->previous.type)->prefix;
    if(parse_prefix_fn == NULL) {
        report_error(compiler, compiler->parser->previous, "Expect expression");
    }
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    parse_prefix_fn(compiler, canAssign);

    while(precedence <= get_rule(compiler->parser->current.type)->precedence) { //PREC_ASSIGNMENT <= PREC_COMPARISON
        advance(compiler);
        parse_fn_t parse_suffix_fn = get_rule(compiler->parser->previous.type)->suffix;
        parse_suffix_fn(compiler, canAssign);
    }

    if(canAssign && match(compiler, TOKEN_EQUAL)){
        report_error(compiler, compiler->parser->previous, "Invalid assigment target.");
    }
}

static void binary(struct bytecode_compiler * compiler, bool can_assign) {
    tokenType_t token_type = compiler->parser->previous.type;

    struct parse_rule * rule = get_rule(token_type);
    parse_precedence(compiler, rule->precedence + 1);

    switch (token_type) {
        case TOKEN_PERCENTAGE: emit_bytecode(compiler, OP_MODULO); break;
        case TOKEN_AMPERSAND: emit_bytecode(compiler, OP_BINARY_OP_AND); break;
        case TOKEN_BAR: emit_bytecode(compiler, OP_BINARY_OP_OR); break;
        case TOKEN_RIGHT_SHIFT: emit_bytecode(compiler, OP_RIGHT_SHIFT); break;
        case TOKEN_LEFT_SHIFT: emit_bytecode(compiler, OP_LEFT_SHIFT); break;
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

static void number(struct bytecode_compiler * compiler, bool can_assign) {
    struct token number_token = compiler->parser->previous;
    double value_as_double = strtod(number_token.start, NULL);

    bool has_decimal = string_contains(number_token.start, number_token.length, '.');
    bool is_8 = value_as_double <= INT8_MAX;
    bool is_16 = !is_8 && value_as_double <= INT16_MAX;

    if(has_decimal || (!is_8 && !is_16)){
        emit_constant(compiler, TO_LOX_VALUE_NUMBER(value_as_double));
    } else if (!has_decimal && value_as_double == 1.0){
        emit_bytecode(compiler, OP_CONST_1);
    } else if (!has_decimal && value_as_double == 2.0){
        emit_bytecode(compiler, OP_CONST_2);
    } else if(!has_decimal && is_8){
        emit_bytecodes(compiler, OP_FAST_CONST_8, (int8_t) value_as_double);
    } else if(!has_decimal && is_16) {
        emit_bytecode(compiler, OP_FAST_CONST_16);
        emit_u16(compiler, (int16_t) value_as_double);
    }
}

static bool is_const_variable(struct bytecode_compiler * compiler, struct token name, struct package * external_package) {
    if (external_package != NULL) { //External package global variable
        struct exported_symbol * external_variable = get_exported_symbol(compiler, external_package, name);
        return external_variable->is_const;
    } else { //Local package global variable
        return contains_trie(&compiler->const_global_variables, name.start, name.length);
    }
}

static void emit_package_constant(struct bytecode_compiler * compiler, lox_value_t value) {
    uint8_t constant_offset = add_constant_to_chunk(current_chunk(compiler), value);
    write_chunk(current_chunk(compiler), OP_PACKAGE_CONST);
    write_chunk(current_chunk(compiler), constant_offset);
}

static void emit_constant(struct bytecode_compiler * compiler, lox_value_t value) {
    uint8_t constant_offset = add_constant_to_chunk(current_chunk(compiler), value);
    write_chunk(current_chunk(compiler), OP_CONSTANT);
    write_chunk(current_chunk(compiler), constant_offset);
}

static void advance(struct bytecode_compiler * compiler) {
    compiler->parser->previous = compiler->parser->current;

    struct token token = next_token_scanner(compiler->scanner);
    compiler->parser->current = token;

    if(token.type == TOKEN_ERROR) {
        report_error(compiler, token, "");
    }
}

static void emit_bytecodes(struct bytecode_compiler * compiler, uint8_t bytecodeA, uint8_t bytecodeB) {
    write_chunk(current_chunk(compiler), bytecodeA);
    write_chunk(current_chunk(compiler), bytecodeB);
}

static void emit_u16(struct bytecode_compiler * compiler, uint16_t value) {
    write_u16_chunk(current_chunk(compiler), value);
}

static void emit_bytecode(struct bytecode_compiler * compiler, uint8_t bytecode) {
    write_chunk(current_chunk(compiler), bytecode);
}

static void consume(struct bytecode_compiler * compiler, tokenType_t expected_token_type, char * error_message) {
    if(compiler->parser->current.type == expected_token_type) {
        advance(compiler);
        return;
    }

    report_error(compiler, compiler->parser->current, error_message);
}

static void report_error(struct bytecode_compiler * compiler, struct token token, char * message) {
    fprintf(stderr, "[line %d] Error", token.line);
    if (token.type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token.type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\n", message);
    exit(1);
}

static struct bytecode_compiler * alloc_compiler(scope_type_t scope, char * package_name, bool is_standalone_mode) {
    struct bytecode_compiler * compiler = NATIVE_LOX_MALLOC(sizeof(struct bytecode_compiler));
    compiler->package = add_package_to_compiled_packages(package_name, strlen(package_name), is_standalone_mode);
    init_compiler(compiler, scope, NULL); //Parent bytecode_compiler null, this current_function will be the first one to be called
    compiler->scanner = NATIVE_LOX_MALLOC(sizeof(struct scanner));
    compiler->parser = NATIVE_LOX_MALLOC(sizeof(struct parser));
    compiler->parser->has_error = false;
    compiler->is_standalone_mode = is_standalone_mode;
    init_trie_list(&compiler->const_global_variables, NATIVE_LOX_ALLOCATOR());
    compiler->compiling_set_array_element_operation = false;

    return compiler;
}

static void emit_exit_monitor(struct bytecode_compiler * compiler) {
    monitor_number_t monitor_number_to_exit = *--compiler->last_monitor_entered;
    emit_bytecodes(compiler, OP_EXIT_MONITOR, monitor_number_to_exit);
}

static void emit_exit_all_monitors(struct bytecode_compiler * compiler) {
    for(monitor_number_t * monitor_number_to_exit = compiler->last_monitor_entered - 1;
        monitor_number_to_exit >= &compiler->monitor_numbers_entered[0];
        --monitor_number_to_exit) {
        emit_bytecodes(compiler, OP_EXIT_MONITOR, * monitor_number_to_exit);
    }
}

static void emit_enter_monitor(struct bytecode_compiler * compiler) {
    if(compiler->current_scope == SCOPE_PACKAGE){
        report_error(compiler, compiler->parser->previous, "Only sync can be called at a function level");
    }
    monitor_number_t monitor_number_to_enter = compiler->last_monitor_allocated_number++;
    if(monitor_number_to_enter >= MAX_MONITORS_PER_FUNCTION){
        report_error(compiler, compiler->parser->previous, "Max monitors in function reached");
    }

    *(compiler->last_monitor_entered++) = monitor_number_to_enter;
    emit_bytecodes(compiler, OP_ENTER_MONITOR, monitor_number_to_enter);
}

static struct package * add_package_to_compiled_packages(char * package_import_name, int package_import_name_length, bool is_stand_alone_mode) {
    struct substring package_substring = read_package_name(package_import_name, package_import_name_length);
    struct package * package_in_compiled_packages = find_trie(compiled_packages, start_substring(package_substring), length_substring(package_substring));

    //If it is not found, it means it is a local package -> a path is used value_as an import name
    if(package_in_compiled_packages == NULL) {
        struct package * new_package = alloc_package();
        char * package_string = add_substring_to_global_string_pool(package_substring, false).string_object->chars;
        new_package->state = PENDING_COMPILATION;
        new_package->name = package_string;
        if(!is_stand_alone_mode) { //If used in standalone mode no local packages is allowed to use
            new_package->absolute_path = import_name_to_absolute_path(package_import_name, package_import_name_length);
        }

        put_trie(compiled_packages, package_string, strlen(package_string), new_package);

        return new_package;
    } else {
        return package_in_compiled_packages;
    }
}

static void init_compiler(struct bytecode_compiler * compiler, scope_type_t scope_type, struct bytecode_compiler * parent_compiler) {
    compiler->current_function = alloc_function();
    init_trie_list(&compiler->function_call_list, NATIVE_LOX_ALLOCATOR());
    init_trie_list(&compiler->const_global_variables, NATIVE_LOX_ALLOCATOR());

    if(scope_type == SCOPE_FUNCTION) {
        compiler->package = parent_compiler->package;
        compiler->scanner = parent_compiler->scanner;
        compiler->parser = parent_compiler->parser;
        compiler->current_function->name = copy_chars_to_string_object(parent_compiler->parser->previous.start,
                                                                       parent_compiler->parser->previous.length);
    } else {
        compiler->package->main_function = compiler->current_function;
    }

    compiler->current_scope = scope_type;

    compiler->local_count = 0;
    compiler->local_depth = 0;
    compiler->max_locals = 0;

    struct local * local = &compiler->locals[compiler->local_count++];
    local->name.length = 0;
    local->name.start = "";
    local->depth = 0;

    compiler->compiling_extermal_symbol_call = false;
    compiler->package_of_external_symbol = NULL;
    compiler->compiling_parallel_call = false;
    compiler->compiling_set_operation = false;
    compiler->compiling_inline_call = false;
    compiler->compiling_set_array_element_operation = false;
    compiler->compiling_new_local_set_body = -2;

    compiler->function_calls = NULL;

    memset(&compiler->monitor_numbers_entered, 0, sizeof(monitor_number_t) * MAX_MONITORS_PER_FUNCTION);
    compiler->last_monitor_entered = &compiler->monitor_numbers_entered[0];
    compiler->last_monitor_allocated_number = 0;

    init_hash_table(&compiler->defined_functions, NATIVE_LOX_ALLOCATOR());

    compiler->function_calls = NULL;

    compiler->next_package_id = 0;
}

static void string(struct bytecode_compiler * compiler, bool can_assign) {
    char * string_ptr = compiler->parser->previous.start + 1;
    int string_length = compiler->parser->previous.length - 2;

    struct string_pool_add_result add_result = add_to_global_string_pool(string_ptr, string_length, false);

    emit_constant(compiler, TO_LOX_VALUE_OBJECT(add_result.string_object));
}

static bool free_trie_node_key_string(void * key, void * extra_ignored) {
    struct trie_node * trie_node = key;
    NATIVE_LOX_FREE(trie_node->key);
    return true;
}

static void free_compiler(struct bytecode_compiler * compiler) {
    free_scanner(compiler->scanner);
    NATIVE_LOX_FREE(compiler->parser);
    NATIVE_LOX_FREE(compiler->scanner);
    for_each_node(&compiler->function_call_list, NULL, free_trie_node_key_string);
    free_trie_list(&compiler->function_call_list);
    NATIVE_LOX_FREE(compiler);
}

static struct function_object * end_compiler(struct bytecode_compiler * compiler) {
    emit_empty_return(compiler);
    compiler->current_function->function_calls = compiler->function_calls;
    compiler->current_function->n_locals = compiler->max_locals;
    compiler->package->state = PENDING_INITIALIZATION;
    return compiler->current_function;
}

static bool match(struct bytecode_compiler * compiler, tokenType_t type) {
    if(!check(compiler, type)){
        return false;
    }
    advance(compiler);
    return true;
}

static bool check(struct bytecode_compiler * compiler, tokenType_t type) {
    return compiler->parser->current.type == type;
}

static int resolve_local_variable(struct bytecode_compiler * compiler, struct token * name) {
    for(int i = compiler->local_count - 1; i >= 0; i--){
        struct local * local = &compiler->locals[i];
        if(identifiers_equal(&local->name, name)){
            return i;
        }
    }

    return -1;
}

static int add_local_variable(
        struct bytecode_compiler * compiler,
        struct token new_variable_name,
        bool * new_local_variable
) {
    if (compiler->local_depth == 0) {
        return - 1; //We are in a global current_scope
    }

    if (is_variable_already_defined(compiler, new_variable_name)) {
        report_error(compiler, new_variable_name, "variable already defined");
    }

    struct local * local = &compiler->locals[compiler->local_count++];
    local->depth = compiler->local_depth;
    local->name = new_variable_name;

    if (new_local_variable != NULL) {
        *new_local_variable = true;
    }


    compiler->max_locals = MAX(compiler->max_locals, compiler->local_count);

    put_u8_hash_table(&compiler->current_function->local_numbers_to_names, compiler->local_count - 1,
        copy_string(new_variable_name.start, new_variable_name.length));

    return compiler->local_count - 1;
}

static bool is_variable_already_defined(struct bytecode_compiler * compiler, struct token new_variable_name) {
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

static uint8_t add_string_constant(struct bytecode_compiler * compiler, struct token string_token) {
    char * variable_name = string_token.start;
    int variable_name_length = string_token.length;

    struct string_pool_add_result result_add = add_to_global_string_pool(variable_name, variable_name_length, false);

    return add_constant_to_chunk(current_chunk(compiler), TO_LOX_VALUE_OBJECT(result_add.string_object));
}

static void define_global_variable(struct bytecode_compiler * compiler, uint8_t global_constant_offset) {
    if(compiler->local_depth == 0) { // Global current_scope
        emit_bytecodes(compiler, OP_DEFINE_GLOBAL, global_constant_offset);
    }
}

static void begin_scope(struct bytecode_compiler * compiler) {
    compiler->local_depth++;
}

static void end_scope(struct bytecode_compiler * compiler) {
    compiler->local_depth--;

    while(compiler->local_count > 0 && compiler->locals[compiler->local_count - 1].depth > compiler->local_depth) {
        compiler->local_count--;
    }
}

static struct chunk * current_chunk(struct bytecode_compiler * compiler) {
    return compiler->current_function->chunk;
}

static void emit_empty_return(struct bytecode_compiler * compiler) {
    emit_bytecodes(compiler, OP_NIL, OP_RETURN);
}

static struct exported_symbol * get_exported_symbol(struct bytecode_compiler * compiler, struct package * external_package, struct token variable_name) {
    struct exported_symbol * exported_var = find_trie(&external_package->exported_symbols, variable_name.start, variable_name.length);
    if(exported_var == NULL){
        report_error(compiler, variable_name, "Variable exported by external_package not found");
    }

    return exported_var;
}

static struct struct_definition_object * get_struct_definition(struct package * package, struct token name) {
    return find_trie(&package->struct_definitions, name.start, name.length);
}

static void add_exported_symbol(struct bytecode_compiler * compiler, struct exported_symbol * exported_symbol, struct token name) {
    bool already_defined = !put_trie(&compiler->package->exported_symbols, name.start, name.length, exported_symbol);
    if(already_defined){
        report_error(compiler, name, "Symbol already defined");
    }
}

static struct function_call * create_current_function_call(struct bytecode_compiler * bytecode_compiler) {
    struct function_call * current_function_call = alloc_function_call();
    current_function_call->is_inlined = bytecode_compiler->compiling_inline_call;
    current_function_call->function_name = bytecode_compiler->current_function_call_name;
    current_function_call->package = bytecode_compiler->package_of_external_symbol != NULL ?
                                     bytecode_compiler->package_of_external_symbol :
                                     bytecode_compiler->package;
    current_function_call->call_bytecode_index = bytecode_compiler->current_function->chunk->in_use - 3;

    return current_function_call;
}

static void add_current_function_call(struct bytecode_compiler * bytecode_compiler) {
    struct function_call * current_function_call = create_current_function_call(bytecode_compiler);
    add_function_call(bytecode_compiler, current_function_call);
}

static void add_function_call(struct bytecode_compiler * bytecode_compiler, struct function_call * function_call) {
    if(bytecode_compiler->function_calls != NULL){
        struct function_call * prev_function_call = bytecode_compiler->function_calls;
        function_call->prev = prev_function_call;
    }

    bytecode_compiler->function_calls = function_call;
}