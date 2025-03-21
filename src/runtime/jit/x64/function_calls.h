#pragma once

#include "runtime/jit/x64/modes/jit_mode_switch_info.h"
#include "runtime/jit/x64/modes/mode.h"
#include "runtime/jit/x64/jit_stack.h"
#include "runtime/jit/x64/opcodes.h"


#include "shared/types/function_object.h"

#define SWITCH_BACK_TO_PREV_MODE_AFTER_CALL 1
#define KEEP_MODE_AFTER_CALL 2
#define DONT_SWITCH_MODES 3

#ifdef __WIN32
static register_t caller_saved_registers[] = {
        RCX,
        RDX,
};
#else
static register_t caller_saved_registers[] = {};
#endif

//We use R10 to store the function address
#ifdef _WIN32
static register_t args_call_convention[] = {
        RCX,
        RDX,
        R8,
        R9,
};
#else
static register_t args_call_convention[] = {
        RDI,
        RSI,
        RDX,
        RCX,
};
#endif

struct jit_compiler;

//As every operating system in x64 have different calling convetions when calling an already compiled C function
//We need some way to abstract ourselves from these details. So when we need to call a C function, we call this function
//which emits the native instructions to call it
uint16_t call_external_c_function(
        struct jit_compiler * jit_compiler,
        jit_mode_t function_mode,
        int mode_switch_config,
        struct operand function_address,
        int n_arguments,
        ... //Operands
        );
