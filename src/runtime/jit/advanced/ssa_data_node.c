#include "ssa_data_node.h"

void * allocate_ssa_data_node(ssa_data_node_type type, size_t struct_size_bytes) {
    struct ssa_data_node * ssa_control_node = malloc(struct_size_bytes);
    memset(ssa_control_node, 0, struct_size_bytes);
    ssa_control_node->type = type;
    return ssa_control_node;
}

profile_data_type_t get_produced_type_ssa_data(
        struct function_profile_data * function_profile,
        struct ssa_data_node * start_node
) {
    switch (start_node->type) {
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local_node = (struct ssa_data_get_local_node *) start_node;
            return get_local_node->type;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            struct ssa_data_get_global_node * get_global = (struct ssa_data_get_global_node *) start_node;
            struct string_object * global_variable_name = get_global->name;
            struct package * package = get_global->package;
            const struct trie_list * constants_variable_names = &package->const_variables;

            if(contains_trie(constants_variable_names, global_variable_name->chars, global_variable_name->length)) {
                lox_value_t global_value;
                get_hash_table(&package->global_variables, global_variable_name, &global_value);
                return lox_value_to_profile_type(global_value);
            } else {
                return PROFILE_DATA_TYPE_ANY;
            }
        }
        case SSA_DATA_NODE_TYPE_ARITHMETIC: {
            struct ssa_data_arithmetic_node * arithmetic_node = (struct ssa_data_arithmetic_node *) start_node;
            profile_data_type_t right_type = arithmetic_node->right_type;
            profile_data_type_t left_type = arithmetic_node->left_type;

            if(left_type == right_type){
                return left_type;
            } else if ((left_type == PROFILE_DATA_TYPE_I64 && right_type == PROFILE_DATA_TYPE_F64) ||
                       (left_type == PROFILE_DATA_TYPE_F64 && right_type == PROFILE_DATA_TYPE_I64)) {
                return PROFILE_DATA_TYPE_F64;
            } else if (left_type == PROFILE_DATA_TYPE_STRING || right_type == PROFILE_DATA_TYPE_STRING) {
                return PROFILE_DATA_TYPE_STRING;
            } else {
                return PROFILE_DATA_TYPE_ANY;
            }
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * constant_node = (struct ssa_data_constant_node *) start_node;
            return constant_node->type;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) start_node;
            return get_produced_type_ssa_data(function_profile, unary_node->unary_value_node);
        }
        case SSA_DATA_NODE_TYPE_COMPARATION: {
            return PROFILE_DATA_TYPE_BOOLEAN;
        }
        case SSA_DATA_NODE_TYPE_CALL:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
        default:
            return PROFILE_DATA_TYPE_ANY;
    }
}