#pragma once

#include "shared/utils/collections/ptr_arraylist.h"
#include "shared.h"

#define MAX_JUMPS_REFERENCES_TO_LINE 32

//This struct allow us to register jumps offsets address pending to resolve per each pending_bytecode instruction
//This struct is only used when dealing with forward jump instrutions like OP_JUMP or OP_JUMP_IF_FALSE

//Sometimes when we are compiling pending_bytecode to native code or inlining functions, we need to resolve jump offsets, but as we haven't
//compiled the whole code we don't know where to jump. This happens for example when we are jit compiling OP_JUMP instruction,
//where we don't know what native offset to jump since we haven't compiled the whole code. The way to solve is that we write the jump
//instruction with an emtpy offset, then we register the pending_bytecode index instruction to jump in this struct with the emtpy jump offset address
//When we compile each pending_bytecode instrution we check if there is some jump pending to resolve which points to this pending_bytecode instruction
struct pending_jumps_to_resolve {
    //Mapping pending_bytecode index to pending_jump_to_resolve
    //Contains struct pending_jump_to_resolve pointer
    struct ptr_arraylist pending;
};

struct pending_jump_to_resolve {
    //Maybe an instruction can be referenced by multiple other jump instructions
    void * pending_resolve_data[MAX_JUMPS_REFERENCES_TO_LINE];
    int in_use;
};

void init_pending_jumps_to_resolve(struct pending_jumps_to_resolve *, int size);
void free_pending_jumps_to_resolve(struct pending_jumps_to_resolve *);

bool add_pending_jump_to_resolve(struct pending_jumps_to_resolve *, int jump_bytecode_index, void * data);

struct pending_jump_to_resolve get_pending_jump_to_resolve(struct pending_jumps_to_resolve *, int bytecode_index);