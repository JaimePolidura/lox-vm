#include "jit_info.h"

void init_jit_info(struct jit_info * jit_info) {
    jit_info->state = BYTECODE;
    jit_info->n_function_calls = 0;
    jit_info->n_function_calls_jit_compiled = 0;
    jit_info->n_times_called = 0;
}

bool increase_call_count_function(struct jit_info * jit_info) {
    int actual_call_count = atomic_fetch_add(&jit_info->n_times_called, 1) + 1;

    return actual_call_count >= MIN_CALLS_TO_JIT_COMPILE &&
            jit_info->n_function_calls_jit_compiled == jit_info->n_function_calls;
}