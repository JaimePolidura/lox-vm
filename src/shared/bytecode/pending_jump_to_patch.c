#include "pending_jump_to_patch.h"

static struct pending_jump_to_patch * alloc_pending_jump_to_patch();

void init_pending_jumps_to_patch(struct pending_jumps_to_patch * pending_path_jump, int size) {
    init_ptr_arraylist(&pending_path_jump->pending);
    resize_ptr_arraylist(&pending_path_jump->pending, size * sizeof(struct pending_jump_to_patch));
}

void free_pending_jumps_to_patch(struct pending_jumps_to_patch * pending_jumps_to_patch) {
    for(int i = 0; i < pending_jumps_to_patch->pending.in_use; i++){
        if(pending_jumps_to_patch->pending.values[i] != NULL){
            free(pending_jumps_to_patch->pending.values[i]);
        }
    }

    free_ptr_arraylist(&pending_jumps_to_patch->pending);
}

bool add_pending_jump_to_patch(struct pending_jumps_to_patch * pending_jumps_to_patch, int jump_bytecode_index, void * data) {
    struct pending_jump_to_patch * pending_jump_to_patch = pending_jumps_to_patch->pending.values[jump_bytecode_index];
    if(pending_jump_to_patch == NULL){
        pending_jump_to_patch = alloc_pending_jump_to_patch();
        pending_jumps_to_patch->pending.values[jump_bytecode_index] = pending_jump_to_patch;
    }

    int index_to_add = pending_jump_to_patch->in_use;
    bool some_free_slot_found = index_to_add < MAX_JUMPS_REFERENCES_TO_LINE;
    if(some_free_slot_found) {
        pending_jump_to_patch->pending_patch_data[index_to_add] = data;
        pending_jump_to_patch->in_use++;
    }

    return some_free_slot_found;
}

struct pending_jump_to_patch get_pending_jump_to_patch(struct pending_jumps_to_patch * pending_jumps, int bytecode_index) {
    struct pending_jump_to_patch * pending_jump_to_patch = pending_jumps->pending.values[bytecode_index];

    if (pending_jump_to_patch == NULL) {
        return (struct pending_jump_to_patch) {.in_use = 0};
    } else {
        return *pending_jump_to_patch;
    }
}

static struct pending_jump_to_patch * alloc_pending_jump_to_patch() {
    struct pending_jump_to_patch * pending_jump_to_patch = malloc(sizeof(struct pending_jump_to_patch));
    memset(pending_jump_to_patch->pending_patch_data, 0, MAX_JUMPS_REFERENCES_TO_LINE);
    pending_jump_to_patch->in_use = 0;
    return pending_jump_to_patch;
}