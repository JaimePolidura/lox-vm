#pragma once

#include "runtime/profiler/profile_data.h"
#include "runtime/threads/vm_thread.h"

#include "shared/types/function_object.h"
#include "shared/bytecode/bytecode.h"
#include "shared/utils/utils.h"
#include "shared.h"

void profile_instruction_profiler(uint8_t * pc, struct function_object *);

bool can_jit_compile_profiler(struct function_object *);