#pragma once

#include "shared.h"
#include "chunk.h"
#include "compiler/bytecode.h"
#include "shared/types/string_object.h"
#include "shared/types/function_object.h"

void disassemble_chunk(struct chunk * chunk);
int disassemble_chunk_instruction(struct chunk * chunk, int offset);