#include "pending_jump_to_resolve.h"

static struct pending_jump_to_resolve * alloc_pending_jump_to_resolve();

void init_pending_jumps_to_resolve(struct pending_jumps_to_resolve * pending_jumps_to_resolve, int size) {
    init_ptr_arraylist(&pending_jumps_to_resolve->pending, NATIVE_LOX_ALLOCATOR());
    resize_ptr_arraylist(&pending_jumps_to_resolve->pending, size * sizeof(void *));
}

void free_pending_jumps_to_resolve(struct pending_jumps_to_resolve * pending_jumps_to_resolve) {
    for(int i = 0; i < pending_jumps_to_resolve->pending.in_use; i++){
        if(pending_jumps_to_resolve->pending.values[i] != NULL){
            NATIVE_LOX_FREE(pending_jumps_to_resolve->pending.values[i]);
        }
    }

    free_ptr_arraylist(&pending_jumps_to_resolve->pending);
}

bool add_pending_jump_to_resolve(struct pending_jumps_to_resolve * pending_jumps_to_resolve, int jump_bytecode_index, void * data) {
    struct pending_jump_to_resolve * pending_jump_to_resolve = pending_jumps_to_resolve->pending.values[jump_bytecode_index];
    if(pending_jump_to_resolve == NULL){
        pending_jump_to_resolve = alloc_pending_jump_to_resolve();
        pending_jumps_to_resolve->pending.values[jump_bytecode_index] = pending_jump_to_resolve;
    }

    int index_to_add = pending_jump_to_resolve->in_use;
    bool some_free_slot_found = index_to_add < MAX_JUMPS_REFERENCES_TO_LINE;
    if(some_free_slot_found) {
        pending_jump_to_resolve->pending_resolve_data[index_to_add] = data;
        pending_jump_to_resolve->in_use++;
    }

    return some_free_slot_found;
}

struct pending_jump_to_resolve get_pending_jump_to_resolve(struct pending_jumps_to_resolve * pending_jumps_to_resolve, int bytecode_index) {
    struct pending_jump_to_resolve * pending_jump_to_resolve = pending_jumps_to_resolve->pending.values[bytecode_index];

    if (pending_jump_to_resolve == NULL) {
        return (struct pending_jump_to_resolve) {.in_use = 0};
    } else {
        return *pending_jump_to_resolve;
    }
}

static struct pending_jump_to_resolve * alloc_pending_jump_to_resolve() {
    struct pending_jump_to_resolve * pending_jump_to_resolve = NATIVE_LOX_MALLOC(sizeof(struct pending_jump_to_resolve));
    memset(pending_jump_to_resolve->pending_resolve_data, 0, MAX_JUMPS_REFERENCES_TO_LINE);
    pending_jump_to_resolve->in_use = 0;
    return pending_jump_to_resolve;
}