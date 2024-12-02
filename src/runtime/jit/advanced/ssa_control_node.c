#include "ssa_control_node.h"

void * allocate_ssa_block_node(
        ssa_control_node_type type,
        size_t struct_size_bytes,
        struct ssa_block * block,
        struct lox_allocator * allocator
) {
    struct ssa_control_node * ssa_control_node = LOX_MALLOC(allocator, struct_size_bytes);
    memset(ssa_control_node, 0, struct_size_bytes);
    ssa_control_node->block = block;
    ssa_control_node->type = type;
    return ssa_control_node;
}

void for_each_data_node_in_control_node(struct ssa_control_node * control_node, void * extra, long options, ssa_data_node_consumer_t consumer) {
    switch(control_node->type){
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL: {
            struct ssa_control_set_local_node * set_local = (struct ssa_control_set_local_node *) control_node;
            if (options == SSA_CONTROL_NODE_OPT_RECURSIVE){
                for_each_ssa_data_node(set_local->new_local_value, (void**) &set_local->new_local_value, extra, consumer);
            } else {
                consumer(NULL, (void**) &set_local->new_local_value, set_local->new_local_value, extra);
            }
            break;
        }
        case SSA_CONTROL_NODE_TYPE_DATA: {
            struct ssa_control_data_node * data_node = (struct ssa_control_data_node *) control_node;
            if(options == SSA_CONTROL_NODE_OPT_RECURSIVE){
                for_each_ssa_data_node(data_node->data, (void**) &data_node->data, extra, consumer);
            } else {
                consumer(NULL, (void**) &data_node->data, data_node->data, extra);
            }
            break;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) control_node;
            if(options == SSA_CONTROL_NODE_OPT_RECURSIVE) {
                for_each_ssa_data_node(return_node->data, (void**) &return_node->data, extra, consumer);
            } else {
                consumer(NULL, (void**) &return_node->data, return_node->data, extra);
            }
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) control_node;
            if (options == SSA_CONTROL_NODE_OPT_RECURSIVE){
                for_each_ssa_data_node(print_node->data, (void**) &print_node->data, extra, consumer);
            } else {
                consumer(NULL, (void**) &print_node->data, print_node->data, extra);
            }
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL: {
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) control_node;
            if(options == SSA_CONTROL_NODE_OPT_RECURSIVE){
                for_each_ssa_data_node(set_global->value_node, (void**) &set_global->value_node, extra, consumer);
            } else {
                consumer(NULL, (void**) &set_global->value_node, set_global->value_node, extra);
            }
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) control_node;
            if(options == SSA_CONTROL_NODE_OPT_RECURSIVE) {
                for_each_ssa_data_node(set_struct_field->new_field_value, (void**) &set_struct_field->new_field_value, extra, consumer);
                for_each_ssa_data_node(set_struct_field->instance, (void**) &set_struct_field->instance, extra, consumer);
            } else {
                consumer(NULL, (void**) &set_struct_field->new_field_value, set_struct_field->new_field_value, extra);
                consumer(NULL, (void**) &set_struct_field->instance, set_struct_field->instance, extra);
            }
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array_element = (struct ssa_control_set_array_element_node *) control_node;
            if(options == SSA_CONTROL_NODE_OPT_RECURSIVE) {
                for_each_ssa_data_node(set_array_element->new_element_value, (void**) &set_array_element->new_element_value, extra, consumer);
                for_each_ssa_data_node(set_array_element->array, (void**) &set_array_element->array, extra, consumer);
            } else {
                consumer(NULL, (void**) &set_array_element->new_element_value, set_array_element->new_element_value, extra);
                consumer(NULL, (void**) &set_array_element->array, set_array_element->array, extra);
            }
            break;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP: {
            struct ssa_control_conditional_jump_node * conditional_jump = (struct ssa_control_conditional_jump_node *) control_node;
            if(options == SSA_CONTROL_NODE_OPT_RECURSIVE){
                for_each_ssa_data_node(conditional_jump->condition, (void**) &conditional_jump->condition, extra, consumer);
            } else {
                consumer(NULL, (void**) &conditional_jump->condition, conditional_jump->condition, extra);
            }
            break;
        }
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME: {
            struct ssa_control_define_ssa_name_node * define_ssa_name = (struct ssa_control_define_ssa_name_node *) control_node;
            if(options == SSA_CONTROL_NODE_OPT_RECURSIVE) {
                for_each_ssa_data_node(define_ssa_name->value, (void**) &define_ssa_name->value, extra, consumer);
            } else {
                consumer(NULL, (void**) &define_ssa_name->value, define_ssa_name->value, extra);
            }
            break;
        }

        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
            break;
    }
}