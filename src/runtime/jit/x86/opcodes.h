#pragma once

#include "shared/utils/collections/u8_arraylist.h"
#include "shared.h"
#include "operands.h"

//a <- b
void emit_mov(struct u8_arraylist * array, struct operand a, struct operand b);
