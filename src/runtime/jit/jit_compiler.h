#pragma once

#include "runtime/jit/jit_compilation_result.h"
#include "runtime/threads/vm_thread.h"

#include "shared/types/function_object.h"
#include "shared/os/os_utils.h"
#include "shared.h"

//Entry point for JIT compiler used by the runtime in vm.c

//Try to jit compile the function.
//The jit compilation might fail if another thread tries to jit compile the same functino concurrently
//Or the jit compilation fails
bool try_jit_compile(struct function_object * function);

void run_jit_compiled(struct function_object * function);