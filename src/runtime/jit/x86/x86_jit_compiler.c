#include "x86_jit_compiler.h"

static struct jit_compiler init_jit_compiler(struct function_object * function);

#define READ_BYTECODE(jit_compiler) (*(jit_compiler)->pc++)
#define READ_U16(jit_compiler) \
    ((jit_compiler)->pc += 2, (uint16_t)(((jit_compiler)->pc[-2] << 8) | (jit_compiler)->pc[-1]))
#define READ_CONSTANT(jit_compiler) (jit_compiler->function_to_compile->chunk.constants.values[READ_BYTECODE(jit_compiler)])

jit_compiled jit_compile(struct function_object * function) {
    struct jit_compiler jit_compiler = init_jit_compiler(function);

    for(;;){
        switch (READ_BYTECODE(&jit_compiler)) {

        }
    }

    return NULL;
}



struct cpu_regs save_cpu_state() {
    return (struct cpu_regs) {
        //TODO
    };
}

void restore_cpu_state(struct cpu_regs * regs) {
    //TODO
}

static struct jit_compiler init_jit_compiler(struct function_object * function) {
    struct jit_compiler compiler;

    compiler.compiled_code = allocate_executable(function->chunk.capacity * 2);
    compiler.function_to_compile = function;
    compiler.pc = function->chunk.code;
    init_stack_list(&compiler.stack);

    return compiler;
}

void end_jit_compiler() {

}