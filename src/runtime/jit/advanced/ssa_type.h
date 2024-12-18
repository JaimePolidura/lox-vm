#pragma once

#include "shared.h"
#include "shared/utils/collections/u64_hash_table.h"
#include "runtime/profiler/profile_data.h"

#define CREATE_SSA_TYPE(type_arg) ((struct ssa_type) {.type = (type_arg)})

struct ssa_type {
    profile_data_type_t type;
    union {
        struct {
            struct struct_definition_object * definition;
            struct u64_hash_table type_by_field_name; //Mapping of string to struct ssa_type
        } struct_instance;
        struct {
            int size; //-1 if unknown
            struct ssa_type * type;
        } array;
    } value;
};