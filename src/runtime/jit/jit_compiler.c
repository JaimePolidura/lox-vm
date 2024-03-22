#include "jit_compiler.h"

extern jit_compiled jit_compile(struct function_object * function);

void try_jit_compile(struct function_object * function) {
    jit_state_t expected_state = BYTECODE;
    if(!atomic_compare_exchange_strong(&function->jit_info.state, &expected_state, JIT_COMPILING)){
        return;
    }

    jit_compiled result = jit_compile(function);

    if(result == NULL){
        //TODO Report error
    }

    function->jit_info.compiled_jit = result;
}