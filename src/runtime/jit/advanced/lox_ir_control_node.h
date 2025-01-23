#pragma once

#include "lox_ir_data_node.h"
#include "lox_ir_guard.h"
#include "runtime/threads/monitor.h"

//Control flow nodes used in SSA IR

typedef enum {
    LOX_IR_CONTROL_NODE_DATA,
    LOX_IR_CONTROL_NODE_RETURN,
    LOX_IR_CONTROL_NODE_PRINT,
    LOX_IR_CONTROL_NODE_ENTER_MONITOR,
    LOX_IR_CONTROL_NODE_EXIT_MONITOR,
    LOX_IR_CONTORL_NODE_SET_GLOBAL,
    LOX_IR_CONTORL_NODE_SET_LOCAL,
    LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD,
    LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT,
    LOX_IR_CONTROL_NODE_LOOP_JUMP,
    LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP,
    LOX_IR_CONTROL_NODE_GUARD,
    //Only used when inserting phi functions in the graph ir creation process
    //It will replace all the nodes with type LOX_IR_CONTORL_NODE_SET_LOCAL in the phi insertion proceess
    LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME,

    //Intrudcued by phi resolution phase, this is not used in optimizations
    LOX_IR_CONTROL_NODE_SET_V_REGISTER,
} lox_ir_control_node_type;

//Fordward reference, so we can use it without including it to avoid cyclical dependencies.
struct lox_ir_block;

#define ALLOC_LOX_IR_CONTROL(type, struct_type, block, allocator) (struct_type *) allocate_lox_ir_control(type, sizeof(struct_type), block, allocator)
void * allocate_lox_ir_control(lox_ir_control_node_type type, size_t size_bytes, struct lox_ir_block * block, struct lox_allocator *);

#define GET_CONDITION_CONDITIONAL_JUMP_LOX_IR_NODE(node) (((struct lox_ir_control_conditional_jump_node *) (node))->condition)
#define GET_DEFINED_SSA_NAME(node) (((struct lox_ir_control_define_ssa_name_node *) (node))->ssa_name)
#define GET_DEFINED_SSA_NAME_VALUE(node) (((struct lox_ir_control_define_ssa_name_node *) (node))->value)

struct lox_ir_control_node {
    lox_ir_control_node_type type;

    struct lox_ir_block * block;

    bool jumps_to_next_node;

    struct lox_ir_control_node * prev;
    struct lox_ir_control_node * next;
};

bool for_each_data_node_in_lox_ir_control(struct lox_ir_control_node *control_node, void *extra, long options, lox_ir_data_node_consumer_t consumer);
struct u64_set get_used_ssa_names_lox_ir_control(struct lox_ir_control_node *control_node, struct lox_allocator *allocator);
//Returns set of pointers to the fields of parent that contains the children pointer. Type: struct lox_ir_data_node **
struct u64_set get_children_lox_ir_control(struct lox_ir_control_node *control_node);
void mark_as_escaped_lox_ir_control(struct lox_ir_control_node *node);
bool is_marked_as_escaped_lox_ir_control(struct lox_ir_control_node *node);

//OP_SET_LOCAL
struct lox_ir_control_set_local_node {
    struct lox_ir_control_node control;
    uint32_t local_number; //Same size as ssa_name
    struct lox_ir_data_node * new_local_value;
};

struct lox_ir_control_data_node {
    struct lox_ir_control_node control;
    struct lox_ir_data_node * data;
};

//OP_SET_GLOBAL
struct lox_ir_control_set_global_node {
    struct lox_ir_control_node control;

    struct package * package;
    struct string_object * name;
    struct lox_ir_data_node * value_node;
};

struct lox_ir_control_print_node {
    struct lox_ir_control_node control;

    struct lox_ir_data_node * data;
};

struct lox_ir_control_return_node {
    struct lox_ir_control_node control;

    struct lox_ir_data_node * data;
};

struct lox_ir_control_enter_monitor_node {
    struct lox_ir_control_node control;

    monitor_number_t monitor_number;
    struct monitor * monitor;
};

struct lox_ir_control_exit_monitor_node {
    struct lox_ir_control_node control;

    monitor_number_t monitor_number;
    struct monitor * monitor;
};

struct lox_ir_control_set_struct_field_node {
    struct lox_ir_control_node control;

    struct string_object * field_name;
    struct lox_ir_data_node * new_field_value;
    struct lox_ir_data_node * instance;
    bool escapes;
};

struct lox_ir_control_set_array_element_node {
    struct lox_ir_control_node control;

    struct lox_ir_data_node * index;
    struct lox_ir_data_node * array;
    struct lox_ir_data_node * new_element_value;
    bool escapes;
    bool requires_range_check;
};

//OP_LOOP
struct lox_ir_control_loop_jump_node {
    struct lox_ir_control_node control;
};

//OP_JUMP_IF_FALSE
struct lox_ir_control_conditional_jump_node {
    struct lox_ir_control_node control;
    struct lox_ir_data_node * condition;
};

//This control_node will be only used when inserting phi functions in the graph ir creation process
//Will replace OP_SET_LOCAL
//The memory outlay of lox_ir_control_define_ssa_name_node and lox_ir_control_set_local_node is the same. We create two
//different struct definitions to note, the differences between the two process in the jit ir graph creation.
//And the same memory outlay so we can replace the control_node in the graph easily, by just changing the control_node type
struct lox_ir_control_define_ssa_name_node {
    struct lox_ir_control_node control;

    struct ssa_name ssa_name;
    struct lox_ir_data_node * value;
};

struct lox_ir_control_guard_node {
    struct lox_ir_control_node control;
    struct lox_ir_guard guard;
};

struct lox_ir_control_set_v_register_node {
    struct lox_ir_control_node control;
    struct lox_ir_data_node * value;
    struct v_register v_register;
};