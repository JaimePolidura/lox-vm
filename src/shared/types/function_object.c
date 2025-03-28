#include "function_object.h"

struct function_object * alloc_function() {
    struct function_object * function_object = ALLOCATE_OBJ(struct function_object, OBJ_FUNCTION);
    init_u8_hash_table(&function_object->local_numbers_to_names);
    function_object->chunk = alloc_chunk();
    function_object->n_arguments = 0;
    function_object->name = NULL;

    for(int i = 0; i < MAX_MONITORS_PER_FUNCTION; i++){
        init_monitor(&function_object->monitors[i]);
    }

    init_object(&function_object->object, OBJ_FUNCTION);

    function_object->state_as.not_profiling.n_calls = 0;

    return function_object;
}

struct instruction_profile_data get_instruction_profile_data_function(
        struct function_object * function,
        struct bytecode_list * bytecode
) {
    return function->state_as.profiling.profile_data.profile_by_instruction_index[bytecode->original_chunk_index];
}

struct type_profile_data get_function_argument_profiled_type(struct function_object * function, int local_number) {
    return function->state_as.profiling.profile_data.arguments_type_profile[local_number];
}