#pragma once

#include "runtime/jit/x64/opcodes.h"

#include "shared/utils/collections/u8_arraylist.h"
#include "shared/types/function_object.h"

#include "shared.h"

void prepare_x64_stack(struct u8_arraylist *, struct function_object *);
void end_x64_stack(struct u8_arraylist *);