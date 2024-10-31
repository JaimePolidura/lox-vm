#include "jit_compilation_result.h"

jit_compiled to_executable_jit_compiation_result(struct jit_compilation_result result) {
    uint8_t * executable_code = allocate_executable(result.compiled_code.in_use);
    memcpy(executable_code, result.compiled_code.values, result.compiled_code.in_use);

    return (jit_compiled) executable_code;
}