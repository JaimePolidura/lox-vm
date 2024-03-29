#pragma once

#include "shared/types/function_object.h"
#include "shared.h"

void try_jit_compile(struct function_object * function);

void run_jit_compiled(struct function_object * function);