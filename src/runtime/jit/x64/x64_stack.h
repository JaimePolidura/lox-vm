#pragma once

#include "runtime/jit/x64/register_allocator.h"
#include "runtime/threads/vm_thread.h"
#include "runtime/jit/x64/opcodes.h"

#include "shared/utils/collections/u8_arraylist.h"
#include "shared/types/function_object.h"

#include "shared.h"

//Used by the jit compiler before and after the compiled bytecode
void emit_prologue_x64_stack(struct u8_arraylist *, struct function_object *);
void emit_epilogue_x64_stack(struct u8_arraylist *, struct function_object *);;