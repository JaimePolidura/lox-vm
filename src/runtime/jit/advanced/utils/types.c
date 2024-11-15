#include "types.h"

profile_data_type_t get_binary_produced_types(profile_data_type_t left_type, profile_data_type_t right_type) {
    if (left_type == right_type) {
        return left_type;
    } else if ((left_type == PROFILE_DATA_TYPE_I64 && right_type == PROFILE_DATA_TYPE_F64) ||
               (left_type == PROFILE_DATA_TYPE_F64 && right_type == PROFILE_DATA_TYPE_I64)) {
        return PROFILE_DATA_TYPE_F64;
    } else if (left_type == PROFILE_DATA_TYPE_STRING || right_type == PROFILE_DATA_TYPE_STRING) {
        return PROFILE_DATA_TYPE_STRING;
    } else {
        return PROFILE_DATA_TYPE_ANY;
    }
}