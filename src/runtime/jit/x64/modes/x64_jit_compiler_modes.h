#pragma once

#include "shared.h"
#include "runtime/jit/x64/x64_jit_compiler.h"
#include "runtime/jit/x64/modes/jit_mode_switch_info.h"

//Functions to transition between different modes in jit_compiler

//Called by the first time jit mode is entered from vm
//Stores "native" rsp & rbp registers
//Setups rcx and rdx to point to lox stack
struct jit_mode_switch_info setup_vm_to_jit_mode(struct jit_compiler *);

//Reconstructs vm stack
//It expects vm stack to remain unchanged
struct jit_mode_switch_info switch_jit_to_vm_mode(struct jit_compiler *);
struct jit_mode_switch_info switch_vm_to_jit_mode(struct jit_compiler *, struct jit_mode_switch_info);