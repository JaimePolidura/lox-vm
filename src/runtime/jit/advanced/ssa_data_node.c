#include "ssa_data_node.h"

static void get_used_locals_recursive(struct u8_arraylist * used, struct ssa_data_node * node);
static void get_assigned_locals_recursive(struct u8_arraylist * used, struct ssa_data_node * node);

void * allocate_ssa_data_node(ssa_data_node_type type, size_t struct_size_bytes, struct bytecode_list * bytecode) {
    struct ssa_data_node * ssa_control_node = malloc(struct_size_bytes);
    memset(ssa_control_node, 0, struct_size_bytes);
    ssa_control_node->original_bytecode = bytecode;
    ssa_control_node->type = type;
    return ssa_control_node;
}

profile_data_type_t get_produced_type_ssa_data(
        struct function_profile_data * function_profile,
        struct ssa_data_node * start_node
) {
    switch (start_node->type) {
        case SSA_DATA_NODE_TYPE_SET_LOCAL: {
            struct ssa_data_set_local_node * set_local_node = (struct ssa_data_set_local_node *) start_node;
            return get_produced_type_ssa_data(function_profile, set_local_node->new_local_value);
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local_node = (struct ssa_data_get_local_node *) start_node;
            return get_local_node->type;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            struct ssa_data_get_global_node * get_global = (struct ssa_data_get_global_node *) start_node;
            struct string_object * global_variable_name = get_global->name;
            struct package * package = get_global->package;
            const struct trie_list * constants_variable_names = &package->const_variables;

            if (contains_trie(constants_variable_names, global_variable_name->chars, global_variable_name->length)) {
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

            if (left_type == right_type) {
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

void get_used_locals(struct u8_arraylist * locals, struct ssa_data_node * node) {
    get_used_locals_recursive(locals, node);
    remove_duplicates_u8_arraylist(locals); //Expect not a lot of local variables being used
}

void get_assgined_locals(struct u8_arraylist * locals, struct ssa_data_node * node) {
    get_assigned_locals_recursive(locals, node);
    remove_duplicates_u8_arraylist(locals); //Expect not a lot of local variables being used
}

static void get_used_locals_recursive(struct u8_arraylist * used, struct ssa_data_node * node) {
    switch (node->type) {
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) node;
            append_u8_arraylist(used, get_local->local_number);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) node;
            for(int i = 0; i < init_struct->definition->n_fields; i++){
                get_used_locals_recursive(used, init_struct->fields_nodes[i]);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) node;
            get_used_locals_recursive(used, get_struct_field->instance_node);
            break;
        }
        case SSA_DATA_NODE_TYPE_COMPARATION: {
            struct ssa_data_comparation_node * comparation = (struct ssa_data_comparation_node *) node;
            get_used_locals_recursive(used, comparation->right);
            get_used_locals_recursive(used, comparation->left);
            break;
        }
        case SSA_DATA_NODE_TYPE_ARITHMETIC: {
            struct ssa_data_arithmetic_node * arithmetic = (struct ssa_data_arithmetic_node *) node;
            get_used_locals_recursive(used, arithmetic->right);
            get_used_locals_recursive(used, arithmetic->left);
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) node;
            get_used_locals_recursive(used, unary->unary_value_node);
            break;
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_control_function_call_node * call_node = (struct ssa_control_function_call_node *) node;
            get_used_locals_recursive(used, call_node->function);
            for(int i = 0; i < call_node->n_arguments; i++){
                get_used_locals_recursive(used, call_node->arguments[i]);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) node;
            get_used_locals_recursive(used, get_array_element->instance);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) node;
            for(int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++){
                get_used_locals_recursive(used, init_array->elememnts_node[i]);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_SET_LOCAL: {
            struct ssa_data_set_local_node * set_local = (struct ssa_data_set_local_node *) node;
            get_used_locals_recursive(used, set_local->new_local_value);
            break;
        }
        case SSA_DATA_NODE_TYPE_CONSTANT:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
            break;
    }
}

static void get_assigned_locals_recursive(struct u8_arraylist * used, struct ssa_data_node * node) {
    switch (node->type) {
        case SSA_DATA_NODE_TYPE_SET_LOCAL: {
            struct ssa_data_set_local_node * set_local = (struct ssa_data_set_local_node *) node;
            append_u8_arraylist(used, set_local->local_number);
            get_assigned_locals_recursive(used, set_local->new_local_value);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) node;
            for(int i = 0; i < init_struct->definition->n_fields; i++){
                get_assigned_locals_recursive(used, init_struct->fields_nodes[i]);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) node;
            get_assigned_locals_recursive(used, get_struct_field->instance_node);
            break;
        }
        case SSA_DATA_NODE_TYPE_COMPARATION: {
            struct ssa_data_comparation_node * comparation = (struct ssa_data_comparation_node *) node;
            get_assigned_locals_recursive(used, comparation->right);
            get_assigned_locals_recursive(used, comparation->left);
            break;
        }
        case SSA_DATA_NODE_TYPE_ARITHMETIC: {
            struct ssa_data_arithmetic_node * arithmetic = (struct ssa_data_arithmetic_node *) node;
            get_assigned_locals_recursive(used, arithmetic->right);
            get_assigned_locals_recursive(used, arithmetic->left);
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) node;
            get_assigned_locals_recursive(used, unary->unary_value_node);
            break;
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_control_function_call_node * call_node = (struct ssa_control_function_call_node *) node;
            get_assigned_locals_recursive(used, call_node->function);
            for(int i = 0; i < call_node->n_arguments; i++){
                get_assigned_locals_recursive(used, call_node->arguments[i]);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) node;
            get_assigned_locals_recursive(used, get_array_element->instance);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) node;
            for(int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++){
                get_assigned_locals_recursive(used, init_array->elememnts_node[i]);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        case SSA_DATA_NODE_TYPE_CONSTANT:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
            break;
    }
}