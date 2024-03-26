#pragma once

#include "shared.h"
#include "chunk.h"
#include "compiler/bytecode.h"
#include "shared/types/string_object.h"
#include "shared/types/function_object.h"

void disassemble_chunk(const struct chunk * chunk);
int disassemble_chunk_instruction(const struct chunk * chunk, int offset);