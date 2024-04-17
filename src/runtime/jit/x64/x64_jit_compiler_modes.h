#pragma once

#include "shared.h"
#include "runtime/jit/x64/x64_jit_compiler.h"

//Called by the first time jit mode is entered from vm
//Stores "native" rsp & rbp registers
//Setups lox stack
void setup_vm_to_jit_mode(struct jit_compiler * jit_compiler);

//Saves the self-thread pointer, rsp & rbp into vm_thread's jit_runtime_info
//Restores native rsp & rbp
//Pushes vm_thread's jit_runtime_info into the native stack
void switch_jit_to_native_mode(struct jit_compiler * jit_compiler);
void switch_native_to_jit_mode(struct jit_compiler * jit_compiler);


void switch_vm_to_jit_mode(struct jit_compiler * jit_compiler);