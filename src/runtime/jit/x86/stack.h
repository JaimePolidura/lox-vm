#pragma once

#include "shared.h"
#include "shared/utils/collections/u8_arraylist.h"
#include "runtime/jit/x86/opcodes.h"
#include "shared/package.h"

void prepare_x64_stack(struct u8_arraylist *);
void end_x64_stack(struct u8_arraylist *);