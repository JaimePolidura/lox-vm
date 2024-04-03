#include "pending_path_jump.h"

static void init_pending_path_jump(struct pending_path_jump * pending_path_jump);
static int get_next_index(struct pending_path_jump * pending_path_jump);

struct pending_path_jump * alloc_pending_path_jump() {
    struct pending_path_jump * pending_path_jump = malloc(sizeof(struct pending_path_jump));
    init_pending_path_jump(pending_path_jump);
    return pending_path_jump;
}

static void init_pending_path_jump(struct pending_path_jump * pending_path_jump) {
    memset(&pending_path_jump->compiled_native_jmp_offset_index, 0, MAX_JUMPS_REFERENCES_TO_LINE * sizeof(uint16_t));
}

bool add_pending_path_jump(struct pending_path_jump * pending_path_jump,
                           uint16_t compiled_native_jmp_offset_index) {
    int index_to_add = get_next_index(pending_path_jump);
    if(index_to_add == -1) {
        return false;
    }

    pending_path_jump->compiled_native_jmp_offset_index[index_to_add] = compiled_native_jmp_offset_index;
    return true;
}

static int get_next_index(struct pending_path_jump * pending_path_forward_jump) {
    for(int i = 0; i < MAX_JUMPS_REFERENCES_TO_LINE; i++){
        if(pending_path_forward_jump->compiled_native_jmp_offset_index[i] == 0){
            return i;
        }
    }

    return -1;
}