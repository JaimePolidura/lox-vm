#include "vm.h"

#include "../chunk/chunk_disassembler.h"

struct vm current_vm;

static double pop_and_check_number();
static bool check_boolean();
static interpret_result run();
static void print_stack();
static void runtime_error(char * format, ...);
static lox_value_t values_equal(lox_value_t a, lox_value_t b);
static inline lox_value_t peek(int index_from_top);
static inline void adition();
static void add_heap_object(struct object * object);
static void define_global();
static void read_global();
static void set_global();
static void set_local();
static void get_local();

interpret_result interpret_vm(struct chunk * chunk) {
    current_vm.chunk = chunk;
    current_vm.pc = chunk->code;
    add_all_pool(&current_vm.string_pool, &chunk->compiled_string_pool);
    
    return run();
}

#define READ_BYTE() (*current_vm.pc++)
#define READ_CONSTANT() (current_vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do { \
        double b = pop_and_check_number(); \
        double a = pop_and_check_number(); \
        push_stack_vm(FROM_NUMBER(a op b)); \
    }while(false);

#define COMPARATION_OP(op) \
    do { \
        double b = pop_and_check_number(); \
        double a = pop_and_check_number(); \
        push_stack_vm(FROM_BOOL(a op b)); \
    }while(false);

static interpret_result run() {
    for(;;) {
#ifdef  DEBUG_TRACE_EXECUTION
        disassemble_chunk_instruction(current_vm.chunk, current_vm.stack - current_vm.esp);
        print_stack();
#endif
        switch (READ_BYTE()) {
            case OP_RETURN: print_value(pop_stack_vm()); break;
            case OP_CONSTANT: push_stack_vm(READ_CONSTANT()); break;
            case OP_NEGATE: push_stack_vm(FROM_NUMBER(-pop_and_check_number())); break;
            case OP_ADD: adition(); break;
            case OP_SUB: BINARY_OP(-) break;
            case OP_MUL: BINARY_OP(*) break;
            case OP_DIV: BINARY_OP(/) break;
            case OP_GREATER: COMPARATION_OP(>) break;
            case OP_LESS: COMPARATION_OP(<) break;
            case OP_FALSE: push_stack_vm(FROM_BOOL(false)); break;
            case OP_TRUE: push_stack_vm(FROM_BOOL(true)); break;
            case OP_NIL: push_stack_vm(FROM_NIL()); break;
            case OP_NOT: push_stack_vm(FROM_BOOL(!check_boolean())); break;
            case OP_EQUAL: push_stack_vm(values_equal(pop_stack_vm(), pop_stack_vm())); break;
            case OP_PRINT: print_value(pop_stack_vm()); printf("\n"); break;
            case OP_POP: pop_stack_vm(); break;
            case OP_DEFINE_GLOBAL: define_global(); break;
            case OP_GET_GLOBAL: read_global(); break;
            case OP_SET_GLOBAL: set_global(); break;
            case OP_GET_LOCAL: get_local(); break;
            case OP_SET_LOCAL: set_local(); break;
            case OP_EOF: return INTERPRET_OK;
            default:
                perror("Unhandled bytecode op\n");
                return INTERPRET_RUNTIME_ERROR;
        }
    }
}

static inline void adition() {
    if(IS_NUMBER(peek(0)) + IS_NUMBER(peek(1))) {
        push_stack_vm(FROM_NUMBER(TO_NUMBER(pop_stack_vm()) + TO_NUMBER(pop_stack_vm())));
        return;
    }

    lox_value_t b_value = pop_stack_vm();
    lox_value_t a_value = pop_stack_vm();
    char * a_chars = cast_to_string(a_value);
    char * b_chars = cast_to_string(b_value);
    size_t a_length = strlen(a_chars);
    size_t b_length = strlen(b_chars);

    size_t new_length = a_length + b_length; //Include \0
    char * concatenated = ALLOCATE(char, new_length + 1);
    memcpy(concatenated, a_chars, a_length);
    memcpy(concatenated + a_length, b_chars, b_length);
    concatenated[new_length] = '\0';

    if(a_value.type == VAL_NUMBER){
        free(a_chars);
    }
    if(b_value.type == VAL_NUMBER){
        free(b_chars);
    }

    push_stack_vm(FROM_OBJECT(add_string(concatenated, new_length)));
}

static void define_global() {
    struct string_object * name = TO_STRING(READ_CONSTANT());
    put_hash_table(&current_vm.global_variables, name, peek(0));
    pop_stack_vm();
}

static void read_global() {
    struct string_object * variable_name = TO_STRING(READ_CONSTANT());
    lox_value_t variable_value;
    if(!get_hash_table(&current_vm.global_variables, variable_name, &variable_value)) {
        runtime_error("Undefined variable %s.", variable_name->chars);
    }

    push_stack_vm(variable_value);
}

static void set_global() {
    struct string_object * variable_name = TO_STRING(READ_CONSTANT());
    if(!contains_hash_table(&current_vm.global_variables, variable_name)){
        runtime_error("Cannot assign value to undeclared variable %s", variable_name->chars);
    }

    put_hash_table(&current_vm.global_variables, variable_name, peek(0));
}

static void set_local() {
    uint8_t slot = READ_BYTE();
    current_vm.stack[slot] = peek(0);
}

static void get_local() {
    uint8_t slot = READ_BYTE();
    push_stack_vm(current_vm.stack[slot]);
}

static inline lox_value_t peek(int index_from_top) {
    return *(current_vm.esp - 1 - index_from_top);
}

static double pop_and_check_number() {
    lox_value_t value = pop_stack_vm();
    if(IS_NUMBER(value)) {
        return TO_NUMBER(value);
    } else {
        runtime_error("Operand must be a number.");
        return -1; //Unreachable
    }
}

static bool check_boolean() {
    lox_value_t value = pop_stack_vm();
    if(IS_BOOL(value)) {
        return TO_BOOL(value);
    } else {
        runtime_error("Operand must be a boolean.");
    }
}

static lox_value_t values_equal(lox_value_t a, lox_value_t b) {
    if(a.type != b.type) {
        return FROM_BOOL(false);
    }

    switch (a.type) {
        case VAL_NIL: return FROM_BOOL(true);
        case VAL_NUMBER: return FROM_BOOL(a.as.number == b.as.number);
        case VAL_BOOL: return FROM_BOOL(a.as.boolean == b.as.boolean);
        case VAL_OBJ: return FROM_BOOL(TO_STRING(a)->chars == TO_STRING(b)->chars);
        default:
            runtime_error("Operator '==' not supported for that type");
            return FROM_BOOL(false); //Unreachable, runtime_error executes exit()
    }
}

void push_stack_vm(lox_value_t value) {
    *current_vm.esp = value;
    current_vm.esp++;
}

lox_value_t pop_stack_vm() {
    auto val = *--current_vm.esp;
    return val;
}

void start_vm() {
    current_vm.esp = current_vm.stack; //Reset stack
    current_vm.heap = NULL;
    init_string_pool(&current_vm.string_pool);
    init_hash_table(&current_vm.global_variables);
}

void stop_vm() {
}

static void runtime_error(char * format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = ((uint8_t *) current_vm.esp) - current_vm.chunk->code - 1;
    int line = current_vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    exit(1);
}

static void print_stack() {
    printf("\t");
    for(lox_value_t * value = current_vm.stack; value < current_vm.esp; value++)  {
        printf("[");
        print_value(*value);
        printf("]");
    }
    printf("\n");
}

struct string_object * add_string(char * string_ptr, int length) {
    struct string_pool_add_result add_result = add_string_pool(&current_vm.string_pool, string_ptr, length);
    if(add_result.created_new){
        add_heap_object((struct object *) add_result.string_object);
    }

    return add_result.string_object;
}

static void add_heap_object(struct object * object) {
    object->next = current_vm.heap;
    current_vm.heap = object;
}