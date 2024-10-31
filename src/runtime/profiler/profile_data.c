#include "profile_data.h"

struct instruction_profile_data * alloc_instruction_profile_data() {
    struct instruction_profile_data * instruction_profile_data = malloc(sizeof(struct instruction_profile_data));
    init_alloc_instruction_profile_data(instruction_profile_data);
    return instruction_profile_data;
}

void init_alloc_instruction_profile_data(struct instruction_profile_data * instruction_profile_data) {
    memset(instruction_profile_data, 0, sizeof(struct instruction_profile_data));
}