#pragma once

#include "ssa_data_node.h"
#include "ssa_guard.h"
#include "runtime/threads/monitor.h"

//Control flow nodes used in SSA IR

typedef enum {
    SSA_CONTROL_NODE_TYPE_DATA,
    SSA_CONTROL_NODE_TYPE_RETURN,
    SSA_CONTROL_NODE_TYPE_PRINT,
    SSA_CONTROL_NODE_TYPE_ENTER_MONITOR,
    SSA_CONTROL_NODE_TYPE_EXIT_MONITOR,
    SSA_CONTORL_NODE_TYPE_SET_GLOBAL,
    SSA_CONTORL_NODE_TYPE_SET_LOCAL,
    SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD,
    SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT,
    SSA_CONTROL_NODE_TYPE_LOOP_JUMP,
    SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP,
    SSA_CONTROL_NODE_GUARD,
    //Only used when inserting phi functions in the graph ir creation process
    //It will replace all the nodes with type SSA_CONTORL_NODE_TYPE_SET_LOCAL in the phi insertion proceess
    SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME,
} ssa_control_node_type;

//Fordward reference, so we can use it without including it to avoid cyclical dependencies.
struct ssa_block;

#define ALLOC_SSA_CONTROL_NODE(type, struct_type, block, allocator) (struct_type *) allocate_ssa_block_node(type, sizeof(struct_type), block, allocator)
void * allocate_ssa_block_node(ssa_control_node_type type, size_t size_bytes, struct ssa_block * block, struct lox_allocator *);

#define GET_CONDITION_CONDITIONAL_JUMP_SSA_NODE(node) (((struct ssa_control_conditional_jump_node *) (node))->condition)
#define GET_DEFINED_SSA_NAME(node) (((struct ssa_control_define_ssa_name_node *) (node))->ssa_name)

struct ssa_control_node {
    ssa_control_node_type type;

    struct ssa_block * block;

    bool jumps_to_next_node;

    struct ssa_control_node * prev;
    struct ssa_control_node * next;
};

void for_each_data_node_in_control_node(struct ssa_control_node *, void *, long options, ssa_data_node_consumer_t);
struct u64_set get_used_ssa_names_ssa_control_node(struct ssa_control_node *, struct lox_allocator *);

//OP_SET_LOCAL
struct ssa_control_set_local_node {
    struct ssa_control_node control;
    uint32_t local_number; //Same size as ssa_name
    struct ssa_data_node * new_local_value;
};

struct ssa_control_data_node {
    struct ssa_control_node control;
    struct ssa_data_node * data;
};

//OP_SET_GLOBAL
struct ssa_control_set_global_node {
    struct ssa_control_node control;

    struct package * package;
    struct string_object * name;
    struct ssa_data_node * value_node;
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

    monitor_number_t monitor_number;
    struct monitor * monitor;
};

struct ssa_control_exit_monitor_node {
    struct ssa_control_node control;

    monitor_number_t monitor_number;
    struct monitor * monitor;
};

struct ssa_control_set_struct_field_node {
    struct ssa_control_node control;

    struct string_object * field_name;
    struct ssa_data_node * new_field_value;
    struct ssa_data_node * instance;
};

struct ssa_control_set_array_element_node {
    struct ssa_control_node control;

    struct ssa_data_node * index;
    struct ssa_data_node * array;
    struct ssa_data_node * new_element_value;
};

//OP_LOOP
struct ssa_control_loop_jump_node {
    struct ssa_control_node control;
};

//OP_JUMP_IF_FALSE
struct ssa_control_conditional_jump_node {
    struct ssa_control_node control;
    struct ssa_data_node * condition;
};

//This control_node will be only used when inserting phi functions in the graph ir creation process
//Will replace OP_SET_LOCAL
//The memory outlay of ssa_control_define_ssa_name_node and ssa_control_set_local_node is the same. We create two
//different struct definitions to note, the differences between the two process in the ssa ir graph creation.
//And the same memory outlay so we can replace the control_node in the graph easily, by just changing the control_node type
struct ssa_control_define_ssa_name_node {
    struct ssa_control_node control;

    struct ssa_name ssa_name;
    struct ssa_data_node * value;
};

struct ssa_control_guard_node {
    struct ssa_control_node control;
    struct ssa_guard guard;
};