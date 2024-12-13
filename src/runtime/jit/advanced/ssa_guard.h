#pragma once

#include "shared.h"

typedef enum {
    SSA_GUARD_TYPE_CHECK,
    SSA_GUARD_STRUCT_DEFINITION_TYPE_CHECK,
    SSA_GUARD_VALUE_CHECK
} ssa_guard_type_t;

extern struct ssa_data_node;

struct ssa_guard {
    struct ssa_data_node * value;
    ssa_guard_type_t type;
    uint64_t value_to_compare;
};