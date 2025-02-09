#pragma once

#include "shared.h"
#include "lox_ir_type.h"

typedef enum {
    //Guard input's type is lox value
    LOX_IR_GUARD_TYPE_CHECK,
    //This is like a typecheck but with the aditional jump_to_operand that the struct instance shluld have a specific definition
    LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK,
    //Same as LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK but array type
    LOX_IR_GUARD_ARRAY_TYPE_CHECK,
    //Used in branches. Guard input type is NATIVE_BOOLEAN. It doesn't perform type checking
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
        //The type has lox binary format
        lox_ir_type_t type;
        //Used when type is set to LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK
        struct struct_definition_object * struct_definition;
        //Used when type is set to LOX_IR_GUARD_BOOLEAN_CHECK
        //The guard will fail if the guard's input doesn't match check_true value
        bool check_true;
    } value_to_compare;
};