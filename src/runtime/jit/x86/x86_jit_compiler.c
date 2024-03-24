#include "x86_jit_compiler.h"

struct cpu_regs_state save_cpu_state() {
    return (struct cpu_regs_state) {
        //TODO
    };
}

void restore_cpu_state(struct cpu_regs_state * regs) {
    //TODO
}

static void number_const(struct jit_compiler * jit_compiler, int value);

#define READ_BYTECODE(jit_compiler) (*(jit_compiler)->pc++)
#define READ_U16(jit_compiler) \
    ((jit_compiler)->pc += 2, (uint16_t)(((jit_compiler)->pc[-2] << 8) | (jit_compiler)->pc[-1]))
#define READ_CONSTANT(jit_compiler) (jit_compiler->function_to_compile->chunk.constants.values[READ_BYTECODE(jit_compiler)])

static struct jit_compiler init_jit_compiler(struct function_object * function);
static void add(struct jit_compiler * jit_compiler);
static void sub(struct jit_compiler * jit_compiler);
static void lox_value(struct jit_compiler * jit_compiler, lox_value_t value);
static void comparation(struct jit_compiler * jit_compiler, op_code comparation_opcode);
static void greater(struct jit_compiler * jit_compiler);

static void cast_to_lox_boolean(struct jit_compiler * jit_compiler);
static void number_const(struct jit_compiler * jit_compiler, int value);

jit_compiled jit_compile(struct function_object * function) {
    struct jit_compiler jit_compiler = init_jit_compiler(function);

    for(;;) {
        switch (READ_BYTECODE(&jit_compiler)) {
            case OP_CONST_1: number_const(&jit_compiler, 1); break;
            case OP_CONST_2: number_const(&jit_compiler, 2); break;
            case OP_FAST_CONST_8: number_const(&jit_compiler, READ_BYTECODE(&jit_compiler)); break;
            case OP_FAST_CONST_16: number_const(&jit_compiler, READ_U16(&jit_compiler)); break;
            case OP_ADD: add(&jit_compiler); break;
            case OP_SUB: sub(&jit_compiler); break;
            case OP_POP: pop_register(&jit_compiler.register_allocator); break;
            case OP_TRUE: lox_value(&jit_compiler, TRUE_VALUE); break;
            case OP_FALSE: lox_value(&jit_compiler, FALSE_VALUE); break;
            case OP_NIL: lox_value(&jit_compiler, NIL_VALUE()); break;
            case OP_EQUAL: comparation(&jit_compiler, OP_EQUAL); break;
            case OP_GREATER: comparation(&jit_compiler, OP_GREATER); break;
            case OP_LESS: comparation(&jit_compiler, OP_LESS); break;
        }
    }

    return NULL;
}

static void greater(struct jit_compiler * jit_compiler)  {
    register_t b = pop_register(&jit_compiler->register_allocator);
    register_t a = pop_register(&jit_compiler->register_allocator);
}

static void comparation(struct jit_compiler * jit_compiler, op_code comparation_opcode) {
    register_t b = pop_register(&jit_compiler->register_allocator);
    register_t a = pop_register(&jit_compiler->register_allocator);
    emit_cmp(&jit_compiler->compiled_code, REGISTER_TO_OPERAND(a), REGISTER_TO_OPERAND(b));

    uint8_t next_bytecode = *(jit_compiler->pc + 1);
    if(next_bytecode != OP_JUMP_IF_FALSE && next_bytecode != OP_LOOP){
        switch (comparation_opcode) {
            case OP_EQUAL: emit_sete_al(&jit_compiler->compiled_code); break;
            case OP_GREATER: emit_setg_al(&jit_compiler->compiled_code); break;
            case OP_LESS: emit_setl_al(&jit_compiler->compiled_code); break;
            default: //TODO Panic
        }

        cast_to_lox_boolean(jit_compiler);
    }
}

static void cast_to_lox_boolean(struct jit_compiler * jit_compiler) {
    register_t register_casted_value = push_register(&jit_compiler->register_allocator);

    //If true register_casted_value will hold 1, if false it will hold 0
    emit_al_movzx(&jit_compiler->compiled_code, REGISTER_TO_OPERAND(register_casted_value));

    //If true, register_casted_value will hold 1, if you add 2, you will get the value of TAG_TRUE defined in types.h
    //If false, register_casted_value will hold 0, if you add 2, you will get the value of TAG_FALSE defined in types.h
    emit_add(&jit_compiler->compiled_code, REGISTER_TO_OPERAND(register_casted_value), IMMEDIATE_TO_OPERAND(2));

    emit_or(&jit_compiler->compiled_code, REGISTER_TO_OPERAND(register_casted_value), IMMEDIATE_TO_OPERAND(QUIET_FLOAT_NAN));
}

static void lox_value(struct jit_compiler * jit_compiler, lox_value_t value) {
    register_t reg = push_register(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->compiled_code, REGISTER_TO_OPERAND(reg), IMMEDIATE_TO_OPERAND(value));
}

static void number_const(struct jit_compiler * jit_compiler, int value) {
    register_t reg = push_register(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->compiled_code, REGISTER_TO_OPERAND(reg), IMMEDIATE_TO_OPERAND(value));
}

static void sub(struct jit_compiler * jit_compiler) {
    register_t operandB = pop_register(&jit_compiler->register_allocator);
    register_t operandA = pop_register(&jit_compiler->register_allocator);

    emit_sub(&jit_compiler->compiled_code, REGISTER_TO_OPERAND(operandA), REGISTER_TO_OPERAND(operandB));
    push_register(&jit_compiler->register_allocator);
}

static void add(struct jit_compiler * jit_compiler) {
    register_t operandB = pop_register(&jit_compiler->register_allocator);
    register_t operandA = pop_register(&jit_compiler->register_allocator);

    emit_add(&jit_compiler->compiled_code, REGISTER_TO_OPERAND(operandA), REGISTER_TO_OPERAND(operandB));

    push_register(&jit_compiler->register_allocator);
}

static struct jit_compiler init_jit_compiler(struct function_object * function) {
    struct jit_compiler compiler;

    compiler.function_to_compile = function;
    compiler.pc = function->chunk.code;

    init_u8_arraylist(&compiler.compiled_code);
    init_register_allocator(&compiler.register_allocator);

    return compiler;
}
