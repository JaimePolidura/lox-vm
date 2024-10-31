#pragma once

#include "shared/types/function_object.h"
#include "shared/bytecode/bytecode.h"
#include "profile_data.h"
#include "shared.h"

void profile_instruction_profiler(bytecode_t, struct function_object *);

bool can_jit_compile_profiler(struct function_object *);
