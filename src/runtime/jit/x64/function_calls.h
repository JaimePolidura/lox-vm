#pragma once

#include "runtime/jit/x64/modes/jit_mode_switch_info.h"
#include "runtime/jit/x64/modes/mode.h"
#include "runtime/jit/x64/x64_stack.h"
#include "runtime/jit/x64/opcodes.h"


#include "shared/types/function_object.h"

#define SWITCH_BACK_TO_PREV_MODE_AFTER_CALL 1
#define KEEP_MODE_AFTER_CALL 2
#define DONT_SWITCH_MODES 3

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

uint16_t call_external_c_function(
        struct jit_compiler * jit_compiler,
        jit_mode_t function_mode,
        int mode_switch_config,
        struct operand function_address,
        int n_arguments,
        ... //Operands
        );
