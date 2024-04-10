#pragma once

#include "params.h"
#include "shared.h"

typedef void (*jit_compiled)(void);

typedef enum {
    JIT_BYTECODE, //Not compiled, no thread compiling it
    JIT_COMPILING, //Some thread is jit-compiling it
    JIT_COMPILED, //It has been jit compiled
    JIT_INCOPILABLE, //Error while jit-compiled. It won't get jit compiled again
} jit_state_t;

//JIT Information maintained per struct function_object
//
//This will keep track of runtime information about the function, like Nº times called
//if some thread is jit-compiling it, native-compiled code, etc.
//
//This struct can be accessed & written concurrently
struct jit_info {
    volatile jit_state_t state;

    //Native-compiled code, produced by jit compilation
    volatile jit_compiled compiled_jit;

    //We only want to jit-compile functions that don't dependen (internally call) on other non jit compiled functions
    //We can compile a function that calls another jit compiled function
    //Only jit compilation in a function will occur when n_function_calls == n_function_calls_jit_compiled
    volatile int n_function_calls; //Number of different function calls in this function. This includes local functions & package functions
    volatile int n_function_calls_jit_compiled; //Number of n_function_calls that haven been jit compiled

    //Number of times that this function has been called. Will be incremented atomically by multiple threads
    volatile int n_times_called;
};

void init_jit_info(struct jit_info * jit_info);

//Increases function call count, returns true if it can be
bool increase_call_count_function(struct jit_info * jit_info);