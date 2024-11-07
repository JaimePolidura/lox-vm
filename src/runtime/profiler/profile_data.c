#include "profile_data.h"

static profile_data_type_t get_type_by_type_profile(struct type_profile_data);

profile_data_type_t get_type_by_local_function_profile_data(struct function_profile_data * profile_data, int local_number) {
    struct type_profile_data type_profile = profile_data->local_profile_data[local_number];
}

void init_function_profile_data(struct function_profile_data * function_profile_data, int n_instructions, int n_locals) {
    function_profile_data->data_by_instruction_index = malloc(sizeof(struct instruction_profile_data) * n_instructions);
    for(int i = 0; i < n_instructions; i++) {
        init_instruction_profile_data(&function_profile_data->data_by_instruction_index[i]);
    }
    function_profile_data->local_profile_data = malloc(sizeof(struct type_profile_data) * n_locals);
    for(int i = 0; i < n_locals; i++) {
        init_type_profile_data(&function_profile_data->local_profile_data[i]);
    }
}

void init_type_profile_data(struct type_profile_data * type_profile_data) {
    memset(type_profile_data, 0, sizeof(struct type_profile_data));
}

struct instruction_profile_data * alloc_instruction_profile_data() {
    struct instruction_profile_data * instruction_profile_data = malloc(sizeof(struct instruction_profile_data));
    init_instruction_profile_data(instruction_profile_data);
    return instruction_profile_data;
}

void init_instruction_profile_data(struct instruction_profile_data * instruction_profile_data) {
    memset(instruction_profile_data, 0, sizeof(struct instruction_profile_data));
}

//TODO Improve code
static profile_data_type_t get_type_by_type_profile(struct type_profile_data type_profile) {
    if(type_profile.object > 0 && type_profile.f64 == 0 && type_profile.boolean == 0 &&
        type_profile.nil == 0 && type_profile.string == 0 && type_profile.i64 == 0){
        return PROFILE_DATA_TYPE_OBJECT;
    } else if(type_profile.object == 0 && type_profile.f64 > 0 && type_profile.boolean == 0 &&
              type_profile.nil == 0 && type_profile.string == 0 && type_profile.i64 == 0){
        return PROFILE_DATA_TYPE_F64;
    } else if(type_profile.object == 0 && type_profile.f64 == 0 && type_profile.boolean > 0 &&
              type_profile.nil == 0 && type_profile.string == 0&& type_profile.i64 == 0) {
        return PROFILE_DATA_TYPE_BOOLEAN;
    } else if(type_profile.object == 0 && type_profile.f64 == 0 && type_profile.boolean == 0 &&
             type_profile.nil > 0 && type_profile.string == 0 && type_profile.i64 == 0) {
        return PROFILE_DATA_TYPE_NIL;
    } else if(type_profile.object == 0 && type_profile.f64 == 0 && type_profile.boolean == 0 &&
             type_profile.nil == 0 && type_profile.string > 0 && type_profile.i64 == 0) {
        return PROFILE_DATA_TYPE_STRING;
    } else if(type_profile.object == 0 && type_profile.f64 == 0 && type_profile.boolean == 0 &&
             type_profile.nil == 0 && type_profile.string == 0 && type_profile.i64 > 0) {
        return PROFILE_DATA_TYPE_I64;
    } else {
        return PROFILE_DATA_TYPE_NIL;
    }
}