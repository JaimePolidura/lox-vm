#pragma once

#include "shared.h"
#include "lox_ir_type.h"

typedef enum {
    LOX_IR_GUARD_TYPE_CHECK,
    //This is like a typecheck but with the aditional condition that the struct instance shluld have a specific definition
    LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK,
    //Same as LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK but array type
    LOX_IR_GUARD_ARRAY_TYPE_CHECK,
    //Used in branches
    LOX_IR_GUARD_BOOLEAN_CHECK
} ssa_guard_type_t;

typedef enum {
    LOX_IR_GUARD_FAIL_ACTION_TYPE_SWITCH_TO_INTERPRETER,
    LOX_IR_GUARD_FAIL_ACTION_TYPE_RUNTIME_ERROR,
} lox_ir_guard_action_on_check_failed;

extern struct lox_ir_data_node;

struct lox_ir_guard {
    struct lox_ir_data_node * value;
    ssa_guard_type_t type;
    lox_ir_guard_action_on_check_failed action_on_guard_failed;

    union {
        //Used when type is set to LOX_IR_GUARD_TYPE_CHECK & LOX_IR_GUARD_ARRAY_TYPE_CHECK
        lox_ir_type_t type;
        //Used when type is set to LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK
        struct struct_definition_object * struct_definition;
        //Used when type is set to LOX_IR_GUARD_BOOLEAN_CHECK
        bool check_true;
    } value_to_compare;
};