#pragma once

#include "runtime/jit/jit_compilation_result.h"
#include "runtime/threads/vm_thread.h"

#include "shared/types/function_object.h"
#include "shared/os/os_utils.h"
#include "shared.h"

//Entry point for JIT bytecode_compiler used by the runtime in vm.c

//Try to jit compile the function.
//The jit compilation might fail if another thread tries to jit compile the same functino concurrently
//Or the jit compilation fails
struct jit_compilation_result try_jit_compile(struct function_object * function);

//Runs function_object jit compiled
void run_jit_compiled(struct function_object * function);

//Each vm_thread holds void * x64_jit_runtime_info, which is a per-thread profile_data structure used
//in runtime when running jit-compiled code
void * alloc_jit_runtime_info();