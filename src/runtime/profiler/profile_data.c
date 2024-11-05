#include "profile_data.h"

void init_function_profile_data(struct function_profile_data * function_profile_data, int n_instructions) {
    function_profile_data->data_by_instruction_index = malloc(sizeof(struct function_profile_data) * n_instructions);
    for(int i = 0; i < n_instructions; i++) {
        init_instruction_profile_data(&function_profile_data->data_by_instruction_index[i]);
    }
}

struct instruction_profile_data * alloc_instruction_profile_data() {
    struct instruction_profile_data * instruction_profile_data = malloc(sizeof(struct instruction_profile_data));
    init_instruction_profile_data(instruction_profile_data);
    return instruction_profile_data;
}

void init_instruction_profile_data(struct instruction_profile_data * instruction_profile_data) {
    memset(instruction_profile_data, 0, sizeof(struct instruction_profile_data));
}

profile_data_type_t get_produced_type_profile_data(struct instruction_profile_data * instruction_profile_data) {
    return PROFILE_DATA_TYPE_ANY;
};