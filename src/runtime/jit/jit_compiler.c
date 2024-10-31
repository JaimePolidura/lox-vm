#include "jit_compiler.h"

extern struct jit_compilation_result jit_compile_arch(struct function_object *);
extern void run_jit_compiled_arch(struct function_object * );

static jit_compiled to_executable(struct jit_compilation_result result);

static void print_jit_result(struct jit_compilation_result result) {
    for(int i = 0; i < result.compiled_code.in_use; i++){
        uint8_t a = result.compiled_code.values[i];

        printf("%02X", result.compiled_code.values[i]);
    }

    puts("\n");
}

struct jit_compilation_result try_jit_compile(struct function_object * function) {
    function_state_t expected_state = FUNC_STATE_PROFILING;
    if(!atomic_compare_exchange_strong(&function->state, &expected_state, FUNC_STATE_JIT_COMPILING)){
        return (struct jit_compilation_result) {
            .success = false,
            .failed_beacause_of_concurrent_compilation = true,
        };
    }

    return jit_compile_arch(function);
}

void run_jit_compiled(struct function_object * function) {
    run_jit_compiled_arch(function);
}

static jit_compiled to_executable(struct jit_compilation_result result) {
    uint8_t * executable_code = allocate_executable(result.compiled_code.in_use);
    memcpy(executable_code, result.compiled_code.values, result.compiled_code.in_use);

    return (jit_compiled) executable_code;
}
