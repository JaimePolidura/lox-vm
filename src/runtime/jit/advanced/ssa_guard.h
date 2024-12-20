#pragma once

#include "shared.h"

typedef enum {
    SSA_GUARD_TYPE_CHECK,
    //This is like a typecheck but with the aditional condition that the struct instance shluld have a specific definition
    SSA_GUARD_STRUCT_DEFINITION_TYPE_CHECK,
    //Used in branches
    SSA_GUARD_BOOLEAN_CHECK
} ssa_guard_type_t;

extern struct ssa_data_node;

struct ssa_guard {
    struct ssa_data_node * value;
    ssa_guard_type_t type;
    union {
        profile_data_type_t type;
        struct struct_definition_object * struct_definition;
        bool check_true;
    } value_to_compare;
};