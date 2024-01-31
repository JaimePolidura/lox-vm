#pragma once

#include "../shared.h"
#include "chunk.h"
#include "../bytecode.h"
#include "types/string_object.h"
#include "types/function.h"

void disassemble_chunk(const struct chunk * chunk);
int disassemble_chunk_instruction(const struct chunk * chunk, int offset);

void print_value(lox_value_t value);