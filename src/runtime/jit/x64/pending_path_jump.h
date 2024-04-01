#pragma once

#include "shared.h"

#define MAX_JUMPS_REFERENCES_TO_LINE 32

//Used for keeping track for a given bytecode instruction the unpatched assembly offset of previuos jump instructions
//A single bytecode instruction offset might be referenced by multiple forward jumps, for example OP_JUMP_IF_FALSE in a set of and conditions
//For each of them we maintain compiled_native_jmp_offset_index individual slot will hold the index of the offset in compiled code, so that we know
//where to write the assembly offset
struct pending_path_jump {
    uint16_t compiled_native_jmp_offset_index[MAX_JUMPS_REFERENCES_TO_LINE];
};

void init_pending_path_jump(struct pending_path_jump *);

bool add_pending_path_jump(struct pending_path_jump *, uint16_t);