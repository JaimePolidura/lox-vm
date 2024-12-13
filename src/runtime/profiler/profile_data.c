#include "profile_data.h"

static profile_data_type_t get_type_by_type_profile(struct type_profile_data);

profile_data_type_t lox_value_to_profile_type(lox_value_t lox_value) {
    if (IS_NUMBER(lox_value)) {
        double double_value = AS_NUMBER(lox_value);
        if (has_decimals(double_value)) {
            return PROFILE_DATA_TYPE_F64;
        } else {
            return PROFILE_DATA_TYPE_I64;
        }
    } else if (IS_BOOL(lox_value)) {
        return PROFILE_DATA_TYPE_BOOLEAN;
    } else if (IS_NIL(lox_value)){
        return PROFILE_DATA_TYPE_NIL;
    } else if (IS_OBJECT(lox_value)) {
        switch (OBJECT_TYPE(lox_value)) {
            case OBJ_STRING: return PROFILE_DATA_TYPE_STRING;
            case OBJ_STRUCT_INSTANCE: return PROFILE_DATA_TYPE_STRUCT_INSTANCE;
            case OBJ_ARRAY: return PROFILE_DATA_TYPE_ARRAY;
            default: return PROFILE_DATA_TYPE_ANY;
        }
    } else {
        return PROFILE_DATA_TYPE_ANY;
    }
}

void init_function_profile_data(struct function_profile_data * function_profile_data, int n_instructions, int n_locals) {
    function_profile_data->profile_by_instruction_index = NATIVE_LOX_MALLOC(sizeof(struct instruction_profile_data) * n_instructions);
    for(int i = 0; i < n_instructions; i++) {
        init_instruction_profile_data(&function_profile_data->profile_by_instruction_index[i]);
    }
}

void init_type_profile_data(struct type_profile_data * type_profile_data) {
    memset(type_profile_data, 0, sizeof(struct type_profile_data));
}

struct instruction_profile_data * alloc_instruction_profile_data() {
    struct instruction_profile_data * instruction_profile_data = NATIVE_LOX_MALLOC(sizeof(struct instruction_profile_data));
    init_instruction_profile_data(instruction_profile_data);
    return instruction_profile_data;
}

void init_instruction_profile_data(struct instruction_profile_data * instruction_profile_data) {
    memset(instruction_profile_data, 0, sizeof(struct instruction_profile_data));
}

//TODO Improve code
static profile_data_type_t get_type_by_type_profile(struct type_profile_data type_profile) {
    if(type_profile.struct_instance > 0 && type_profile.array == 0 && type_profile.any == 0 && type_profile.f64 == 0 &&
        type_profile.boolean == 0 && type_profile.nil == 0 && type_profile.string == 0 && type_profile.i64 == 0){
        return PROFILE_DATA_TYPE_STRUCT_INSTANCE;
    } else if(type_profile.struct_instance == 0 && type_profile.array > 0 && type_profile.any == 0 && type_profile.f64 == 0 &&
              type_profile.boolean == 0 && type_profile.nil == 0 && type_profile.string == 0 && type_profile.i64 == 0) {
        return PROFILE_DATA_TYPE_ARRAY;
    } else if (type_profile.struct_instance == 0 && type_profile.array == 0 && type_profile.any > 0 && type_profile.f64 == 0 &&
               type_profile.boolean == 0 && type_profile.nil == 0 && type_profile.string == 0 && type_profile.i64 == 0) {
        return PROFILE_DATA_TYPE_ANY;
    } else if(type_profile.struct_instance == 0 && type_profile.array == 0 && type_profile.any == 0 && type_profile.f64 > 0 &&
              type_profile.boolean == 0 && type_profile.nil == 0 && type_profile.string == 0 && type_profile.i64 == 0){
        return PROFILE_DATA_TYPE_F64;
    } else if(type_profile.struct_instance == 0 && type_profile.array == 0 && type_profile.any == 0 && type_profile.f64 == 0 &&
              type_profile.boolean > 0 && type_profile.nil == 0 && type_profile.string == 0 && type_profile.i64 == 0){
        return PROFILE_DATA_TYPE_BOOLEAN;
    } else if(type_profile.struct_instance == 0 && type_profile.array == 0 && type_profile.any == 0 && type_profile.f64 == 0 &&
              type_profile.boolean == 0 && type_profile.nil > 0 && type_profile.string == 0 && type_profile.i64 == 0) {
        return PROFILE_DATA_TYPE_NIL;
    } else if (type_profile.struct_instance == 0 && type_profile.array == 0 && type_profile.any == 0 && type_profile.f64 == 0 &&
               type_profile.boolean == 0 && type_profile.nil == 0 && type_profile.string > 0 && type_profile.i64 == 0) {
        return PROFILE_DATA_TYPE_STRING;
    } else if (type_profile.struct_instance == 0 && type_profile.array == 0 && type_profile.any == 0 && type_profile.f64 == 0 &&
               type_profile.boolean == 0 && type_profile.nil == 0 && type_profile.string == 0 && type_profile.i64 > 0) {
        return PROFILE_DATA_TYPE_I64;
    } else {
        return PROFILE_DATA_TYPE_ANY;
    }
}

char * profile_data_type_to_string(profile_data_type_t type) {
    switch (type) {
        case PROFILE_DATA_TYPE_I64: return "i64";
        case PROFILE_DATA_TYPE_F64: return "f64";
        case PROFILE_DATA_TYPE_STRING: return "string";
        case PROFILE_DATA_TYPE_BOOLEAN: return "boolean";
        case PROFILE_DATA_TYPE_NIL: return "nil";
        case PROFILE_DATA_TYPE_STRUCT_INSTANCE: return "struct";
        case PROFILE_DATA_TYPE_ARRAY: return "array";
        case PROFILE_DATA_TYPE_ANY: return "any";
    }
}