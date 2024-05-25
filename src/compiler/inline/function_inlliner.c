#include "function_inliner.h"

static void update_local_variables_offset(struct function_object * target, struct chunk * chunk_to_inline);

struct function_inline_result inline_function(
        struct function_object * target,
        int chunk_target_index,
        struct function_object * function_to_inline
) {
    struct chunk * to_inline_chunk = copy_chunk(&function_to_inline->chunk);

    update_local_variables_offset(target, to_inline_chunk);

    return (struct function_inline_result) {};
}

static void update_local_variables_offset(struct function_object * target, struct chunk * chunk_to_inline) {
    // +1 becuase of the return value, which will be stored in a LOCAL VARIABLE
    int offset_to_add = target->n_locals + 1;
    struct chunk_iterator iterator = iterate_chunk(chunk_to_inline);

    while(has_next_chunk_iterator(&iterator)){
        switch (next_instruction_chunk_iterator(&iterator)) {
            case OP_SET_LOCAL:
            case OP_GET_LOCAL:
                uint8_t current_local_value = read_u8_chunk_iterator(&iterator);
                write_u8_chunk_iterator_at(&iterator, 0, current_local_value + offset_to_add);
                break;
            default:
                continue;
        }
    }
}