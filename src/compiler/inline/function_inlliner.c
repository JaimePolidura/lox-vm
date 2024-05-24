#include "function_inliner.h"

static void change_local_variables_number(struct function_object * target, struct chunk * chunk_to_inline);

struct function_inline_result inline_function(
        struct function_object * target,
        int chunk_target_index,
        struct function_object * function_to_inline
) {
    struct chunk * to_inline_chunk = copy_chunk(&function_to_inline->chunk);

    change_local_variables_number(target, to_inline_chunk);

    return (struct function_inline_result) {};
}

static void change_local_variables_number(struct function_object * target, struct chunk * chunk_to_inline) {
    // +1 becuase of the return value, which will be stored in a LOCAL VARIABLE
    int offset_to_add = target->n_locals + 1;
}