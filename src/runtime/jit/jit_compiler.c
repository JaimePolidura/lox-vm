#include "jit_compiler.h"

extern struct jit_compilation_result jit_compile_arch(struct function_object *);
extern void run_jit_compiled_arch(struct function_object * );
extern void * alloc_jit_runtime_info_arch();

static jit_compiled to_executable(struct jit_compilation_result result);

static void print_jit_result(struct jit_compilation_result result) {
    for(int i = 0; i < result.compiled_code.in_use; i++){
        uint8_t a = result.compiled_code.values[i];

        printf("%02X", result.compiled_code.values[i]);
    }

    puts("\n");
}

bool try_jit_compile(struct function_object * function) {
    jit_state_t expected_state = JIT_BYTECODE;
    if(!atomic_compare_exchange_strong(&function->jit_info.state, &expected_state, JIT_COMPILING)){
        return false;
    }

    struct jit_compilation_result result = jit_compile_arch(function);

    print_jit_result(result);

    if(result.success){
        function->jit_info.compiled_jit = to_executable(result);
        COMPILER_BARRIER(); //TODO Use memory barriers
        function->jit_info.state = JIT_COMPILED;
        return true;
    } else {
        function->jit_info.state = JIT_INCOPILABLE;
        return false;
    }
}

void run_jit_compiled(struct function_object * function) {
    run_jit_compiled_arch(function);
}

static jit_compiled to_executable(struct jit_compilation_result result) {
    uint8_t * executable_code = allocate_executable(result.compiled_code.in_use);
    memcpy(executable_code, result.compiled_code.values, result.compiled_code.in_use);

    return (jit_compiled) executable_code;
}

void * alloc_jit_runtime_info() {
    return alloc_jit_runtime_info_arch();
}
