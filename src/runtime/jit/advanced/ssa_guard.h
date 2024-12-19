#pragma once

#include "shared.h"

typedef enum {
    SSA_GUARD_TYPE_CHECK,
    SSA_GUARD_STRUCT_DEFINITION_TYPE_CHECK,
    SSA_GUARD_BOOLEAN_CHECK //Used in branches
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