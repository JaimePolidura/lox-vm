#pragma once

#include "shared/types/function_object.h"
#include "jit_compilation_result.h"
#include "shared.h"

//Implemented by everey specific jit compiler architecture (arm, x64 etc.)

struct jit_compilation_result jit_compile_arch(struct function_object *);

void run_jit_compiled_arch(struct function_object * );