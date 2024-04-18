#pragma once

#include "shared.h"
#include "runtime/jit/x64/x64_jit_compiler.h"

//Functions to transition between different modes in jit_compiler

//Called by the first time jit mode is entered from vm
//Stores "native" rsp & rbp registers
//Setups rsp and rbp to point to lox stack
void setup_vm_to_jit_mode(struct jit_compiler * jit_compiler);

//Saves the self-thread pointer, rsp & rbp into vm_thread's jit_runtime_info
//Restores native rsp & rbp
//Pushes vm_thread's jit_runtime_info into the native stack
void switch_jit_to_native_mode(struct jit_compiler * jit_compiler);
//Pops jit_runtime_info from native stack
//Restores rsp & rbp from jit_runtime_info
//Expects the native stack to be unchanged after the function call to switch_jit_to_native_mode
void switch_native_to_jit_mode(struct jit_compiler * jit_compiler);

//Reconstructs vm stack
//It expects the code that needs vm_thread's stack to not modify it
void switch_jit_to_vm_mode(struct jit_compiler * jit_compiler);
void switch_vm_to_jit_mode(struct jit_compiler * jit_compiler);