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
static void number_const(struct jit_compiler * jit_compiler, int value);
static void add(struct jit_compiler * jit_compiler);
static void sub(struct jit_compiler * jit_compiler);

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
        }
    }

    return NULL;
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
