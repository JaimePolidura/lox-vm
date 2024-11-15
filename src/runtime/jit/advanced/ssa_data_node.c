#include "ssa_data_node.h"

static void for_each_ssa_data_node_recursive(struct ssa_data_node * parent_current, struct ssa_data_node * current_node, void *, ssa_data_node_consumer_t);

void * allocate_ssa_data_node(ssa_data_node_type type, size_t struct_size_bytes, struct bytecode_list * bytecode) {
    struct ssa_data_node * ssa_control_node = malloc(struct_size_bytes);
    memset(ssa_control_node, 0, struct_size_bytes);
    ssa_control_node->produced_type = PROFILE_DATA_TYPE_ANY;
    ssa_control_node->original_bytecode = bytecode;
    ssa_control_node->type = type;
    return ssa_control_node;
}

void get_used_locals_consumer(struct ssa_data_node * _, struct ssa_data_node * current_node, void * extra) {
    struct u8_set * used_locals_set = (struct u8_set *) extra;

    if(current_node->type == SSA_DATA_NODE_TYPE_GET_LOCAL){
        struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) current_node;
        add_u8_set(used_locals_set, get_local->local_number);
    }
}

struct u8_set get_used_locals(struct ssa_data_node * node) {
    struct u8_set used_locals_set;
    init_u8_set(&used_locals_set);

    for_each_ssa_data_node(node, &used_locals_set, &get_used_locals_consumer);

    return used_locals_set;
}

void for_each_ssa_data_node(struct ssa_data_node * node, void * extra, ssa_data_node_consumer_t consumer) {
    for_each_ssa_data_node_recursive(NULL, node, extra, consumer);
}

static void for_each_ssa_data_node_recursive(
        struct ssa_data_node * parent_current,
        struct ssa_data_node * current_node,
        void * extra,
        ssa_data_node_consumer_t consumer
) {
    consumer(parent_current, current_node, extra);

    switch(current_node->type){
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) current_node;
            for(int i = 0; i < init_struct->definition->n_fields; i++){
                for_each_ssa_data_node_recursive(current_node, init_struct->fields_nodes[i], extra, consumer);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) current_node;
            for_each_ssa_data_node_recursive(current_node, get_struct_field->instance_node, extra, consumer);
            break;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current_node;
            for_each_ssa_data_node_recursive(current_node, binary_node->left, extra, consumer);
            for_each_ssa_data_node_recursive(current_node, binary_node->right, extra, consumer);
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) current_node;
            for_each_ssa_data_node_recursive(current_node, unary->unary_value_node, extra, consumer);
            break;
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_control_function_call_node * call_node = (struct ssa_control_function_call_node *) current_node;
            for_each_ssa_data_node_recursive(current_node, call_node->function, extra, consumer);
            for(int i = 0; i < call_node->n_arguments; i++){
                for_each_ssa_data_node_recursive(current_node, call_node->arguments[i], extra, consumer);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) current_node;
            for_each_ssa_data_node_recursive(current_node, get_array_element->instance, extra, consumer);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) current_node;
            for(int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++){
                for_each_ssa_data_node_recursive(current_node, init_array->elememnts_node[i], extra, consumer);
            }
            break;
        }
        default:
            break;
    }
}