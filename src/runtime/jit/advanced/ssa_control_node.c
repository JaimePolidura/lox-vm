#include "ssa_control_node.h"

void * allocate_ssa_block_node(ssa_control_node_type type, size_t struct_size_bytes) {
    struct ssa_control_node * ssa_control_node = malloc(struct_size_bytes);
    memset(ssa_control_node, 0, struct_size_bytes);
    ssa_control_node->type = type;
    return ssa_control_node;
}

void for_each_data_node_in_control_node(struct ssa_control_node * control_node, void * extra, ssa_data_node_consumer_t consumer) {
    switch(control_node->type){
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL: {
            struct ssa_control_set_local_node * set_local = (struct ssa_control_set_local_node *) control_node;
            for_each_ssa_data_node(set_local->new_local_value, (void**) &set_local->new_local_value, extra, consumer);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_DATA: {
            struct ssa_control_data_node * data_node = (struct ssa_control_data_node *) control_node;
            for_each_ssa_data_node(data_node->data, (void**) &data_node->data, extra, consumer);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) control_node;
            for_each_ssa_data_node(return_node->data, (void**) &return_node->data, extra, consumer);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) control_node;
            for_each_ssa_data_node(print_node->data, (void**) &print_node->data, extra, consumer);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL: {
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) control_node;
            for_each_ssa_data_node(set_global->value_node, (void**) &set_global->value_node, extra, consumer);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) control_node;
            for_each_ssa_data_node(set_struct_field->new_field_value, (void**) &set_struct_field->new_field_value, extra, consumer);
            for_each_ssa_data_node(set_struct_field->instance, (void**) &set_struct_field->instance, extra, consumer);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array_element = (struct ssa_control_set_array_element_node *) control_node;
            for_each_ssa_data_node(set_array_element->new_element_value, (void**) &set_array_element->new_element_value, extra, consumer);
            for_each_ssa_data_node(set_array_element->array, (void**) &set_array_element->array, extra, consumer);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP: {
            struct ssa_control_conditional_jump_node * conditional_jump = (struct ssa_control_conditional_jump_node *) control_node;
            for_each_ssa_data_node(conditional_jump->condition, (void**) &conditional_jump->condition, extra, consumer);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME: {
            struct ssa_control_define_ssa_name_node * define_ssa_name = (struct ssa_control_define_ssa_name_node *) control_node;
            for_each_ssa_data_node(define_ssa_name->new_local_value, (void**) &define_ssa_name->new_local_value, extra, consumer);
            break;
        }

        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        case SSA_CONTROL_NODE_TYPE_START:
            break;
    }
}