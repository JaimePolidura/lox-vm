#include "jit_compiler.h"

extern struct jit_compilation_result jit_compile_arch(struct function_object * function);

static jit_compiled to_executable(struct jit_compilation_result result);

void try_jit_compile(struct function_object * function) {
    jit_state_t expected_state = JIT_BYTECODE;
    if(!atomic_compare_exchange_strong(&function->jit_info.state, &expected_state, JIT_COMPILING)){
        return;
    }

    struct jit_compilation_result result = jit_compile_arch(function);

    if(result.success){
        function->jit_info.compiled_jit = to_executable(result);
        COMPILER_BARRIER(); //TODO Use memory barriers
        function->jit_info.state = JIT_COMPILED;
    } else {
        function->jit_info.state = JIT_INCOPILABLE;
    }
}

void run_jit_compiled(struct function_object * function) {
    function->jit_info.compiled_jit();
}

static jit_compiled to_executable(struct jit_compilation_result result) {
    uint8_t * executable_code = allocate_executable(result.compiled_code.in_use);
    memcpy(executable_code, result.compiled_code.values, result.compiled_code.in_use);

    return (jit_compiled) executable_code;
}