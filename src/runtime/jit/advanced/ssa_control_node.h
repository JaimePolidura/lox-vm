#include "ssa_data_node.h"
#include "runtime/threads/monitor.h"

//Control flow nodes used in SSA IR

#define ALLOC_SSA_CONTROL_NODE(type, struct_type) (struct_type *) allocate_ssa_block_node(type, sizeof(struct_type))

typedef enum {
    SSA_CONTROL_NODE_TYPE_START,
    SSA_CONTROL_NODE_TYPE_RETURN,
    SSA_CONTROL_NODE_TYPE_PRINT,
    SSA_CONTROL_NODE_TYPE_ENTER_MONITOR,
    SSA_CONTROL_NODE_TYPE_EXIT_MONITOR,
    SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD,
    SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT,
    SSA_CONTROL_NODE_TYPE_SET_LOOP_JUMP,
    SSA_CONTROL_NODE_TYPE_SET_FUNCTION_CALL,
    SSA_CONTROL_NODE_TYPE_SET_CONDITIONAL_JUMP
} ssa_control_node_type;

struct ssa_control_node {
    ssa_control_node_type type;

    union {
        struct ssa_control_node * next;
        struct {
            struct ssa_control_node * true_branch;
            //If the "if" statement does not contain any else statement, this will be NULL
            struct ssa_control_node * false_branch;
        } branch;
    } next;
};

void * allocate_ssa_block_node(ssa_control_node_type type, size_t size_bytes);

struct ssa_control_start_node {
    ssa_control_node_type type;
};

struct ssa_control_print_node {
    struct ssa_control_node control;

    struct ssa_data_node * data;
};

struct ssa_control_return_node {
    struct ssa_control_node control;

    struct ssa_data_node * data;
};

struct ssa_control_enter_monitor_node {
    struct ssa_control_node control;

    struct monitor * monitor;
};

struct ssa_control_exit_monitor_node {
    struct ssa_control_node control;

    struct monitor * monitor;
};

struct ssa_control_set_struct_field_node {
    struct ssa_control_node control;

    struct string_object * field_name;
    struct ssa_data_node * new_value;
    struct ssa_data_node * instance;
};

struct ssa_control_set_array_element_node {
    struct ssa_control_node control;

    uint16_t index;
    struct ssa_data_node * array;
    struct ssa_data_node * new_element;
};

//OP_LOOP
struct ssa_control_loop_jump_node {
    struct ssa_control_node control;

    struct ssa_control_node * to;
};

struct ssa_control_function_call_node {
    struct ssa_control_node control;

    int n_arguments;
    bool is_parallel;
    struct object * function_obj;
};

//OP_JUMP_IF_FALSE //OP_JUMP
struct ssa_control_conditional_jump_node {
    struct ssa_control_node control;

    struct ssa_data_node * condition;
};
