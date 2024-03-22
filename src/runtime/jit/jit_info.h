#pragma once

#include "params.h"
#include "shared.h"

typedef void (*jit_compiled)(void);

typedef enum {
    BYTECODE,
    JIT_COMPILING,
    JIT_COMPILED
} jit_state_t;

//JIT info maintained per function
struct jit_info {
    volatile jit_state_t state;

    volatile jit_compiled compiled_jit;

    //Only jit compilation in this function will occur when n_function_calls == n_function_calls_jit_compiled
    volatile int n_function_calls; //Number of different function calls. This includes local functions & package functions
    volatile int n_function_calls_jit_compiled; //Number of n_function_calls that haven been jit compiled

    //Number of times that this function has been called. Will be incremented atomically by multiple threads
    volatile int n_times_called;
};

void init_jit_info(struct jit_info * jit_info);

//Increases function call count, returns true if it can be
bool increase_call_count_function(struct jit_info * jit_info);