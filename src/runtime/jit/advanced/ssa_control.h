//Control flow nodes used in SSA IR

#include "ssa_data.h"
#include "runtime/threads/monitor.h"

typedef enum {
    SSA_CONTROL_NODE_TYPE_RETURN,
    SSA_CONTROL_NODE_TYPE_PRINT
} ssa_control_node_type;

struct ssa_control {
    ssa_control_node_type type;
};

struct ssa_control_print {
    struct ssa_control control;

    struct ssa_data * data;
};

struct ssa_control_return {
    struct ssa_control control;

    struct ssa_data * data;
};

struct ssa_control_enter_monitor {
    struct ssa_control control;

    struct monitor * monitor;
};

struct ssa_control_exit_monitor {
    struct ssa_control control;

    struct monitor * monitor;
};

struct ssa_control_set_struct_field {
    struct ssa_control control;

    struct string_object * field_name;
    struct ssa_data * new_value;
    struct ssa_data * instance;
};

struct ssa_control_set_array_element {
    struct ssa_control control;

    uint16_t index;
    struct ssa_data * array;
    struct ssa_data * new_element;
};

//OP_LOOP
struct ssa_control_loop_jump {
    struct ssa_control control;

    struct ssa_control * to;
};

struct ssa_control_function_call {
    struct ssa_control control;

    int n_arguments;
    bool is_parallel;
    struct object * function_obj;
};

//OP_JUMP_IF_FALSE //OP_JUMP
struct ssa_control_condition {
    struct ssa_control control;

    struct ssa_data * condition;
    struct ssa_control * true_branch;
    //If the "if" statement does not contain any else statement, this will be NULL
    struct ssa_control * false_branch;
};