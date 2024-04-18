#pragma once

#include "runtime/jit/x64/opcodes.h"
#include "runtime/jit/x64/x64_stack.h"
#include "runtime/jit/x64/modes/mode.h"

#include "shared/types/function_object.h"

#define SWITCH_BACK_TO_PREV_MODE_AFTER_CALL true
#define KEEP_MODE_AFTER_CALL false

struct jit_compiler;

uint16_t call_external_c_function(
        struct jit_compiler * jit_compiler,
        jit_mode_t function_mode,
        bool restore_mode_after_call,
        struct operand function_address,
        int n_arguments,
        ... //Operands
        );
