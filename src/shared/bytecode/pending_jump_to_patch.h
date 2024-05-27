#pragma once

#include "shared/utils/collections/ptr_arraylist.h"
#include "shared.h"

#define MAX_JUMPS_REFERENCES_TO_LINE 32

struct pending_jumps_to_patch {
    //Mapping bytecode index to pending_jump_to_patch
    //Contains struct pending_jump_to_patch pointer
    struct ptr_arraylist pending;
};

struct pending_jump_to_patch {
    //Maybe an instruction can be referenced by multiple other jump instructions
    void * pending_patch_data[MAX_JUMPS_REFERENCES_TO_LINE];
    int in_use;
};

void init_pending_jumps_to_patch(struct pending_jumps_to_patch *, int size);
void free_pending_jumps_to_patch(struct pending_jumps_to_patch *);

bool add_pending_jump_to_patch(struct pending_jumps_to_patch *, int jump_bytecode_index, void * data);

struct pending_jump_to_patch get_pending_jump_to_patch(struct pending_jumps_to_patch *, int bytecode_index);