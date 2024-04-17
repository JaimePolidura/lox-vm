#pragma once

#include "runtime/jit/x64/register_allocator.h"
#include "runtime/threads/vm_thread.h"
#include "runtime/jit/x64/opcodes.h"

#include "shared/utils/collections/u8_arraylist.h"
#include "shared/types/function_object.h"

#include "shared.h"

//Used by the jit compiler before and after the compiled bytecode
void emit_prologue_x64_stack(struct u8_arraylist *, struct function_object *);
void emit_epilogue_x64_stack(struct u8_arraylist *, struct function_object *);

//These functions are used to change from lox stack to native stack

//Instead of, keep using the native stack pointed by rbp and rsp. We are going to "switch" the stack to point to vm_thread stack
//This is going to simplify argument passing, return values & garbage collection, because we are using the same stack that a function
//running in bytecode is using.

//The only restriction is that we won't be able to use pop and push x64 instructions, since it would override the vm stack in an unwanted way.
//This happens because x64 stack grows to lower address while vm.c stack to higher address.

//We store previous RSP and RBP into RCX & RDX
void switch_to_lox_stack(struct u8_arraylist * code, struct function_object * function);