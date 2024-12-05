#include "ssa_data_node.h"

extern void runtime_panic(char * format, ...);

static void for_each_ssa_data_node_recursive(
        struct ssa_data_node * parent_current,
        void ** parent_current_ptr,
        struct ssa_data_node * current_node,
        void * extra,
        long options,
        ssa_data_node_consumer_t consumer
);

void * allocate_ssa_data_node(
        ssa_data_node_type type,
        size_t struct_size_bytes,
        struct bytecode_list * bytecode,
        struct lox_allocator * allocator
) {
    struct ssa_data_node * ssa_control_node = LOX_MALLOC(allocator, struct_size_bytes);
    memset(ssa_control_node, 0, struct_size_bytes);
    ssa_control_node->produced_type = PROFILE_DATA_TYPE_ANY;
    ssa_control_node->original_bytecode = bytecode;
    ssa_control_node->type = type;
    return ssa_control_node;
}

void get_used_locals_consumer(struct ssa_data_node * _, void ** __, struct ssa_data_node * current_node, void * extra) {
    struct u8_set * used_locals_set = (struct u8_set *) extra;

    if(current_node->type == SSA_DATA_NODE_TYPE_GET_LOCAL){
        struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) current_node;
        add_u8_set(used_locals_set, get_local->local_number);
    }
}

struct u8_set get_used_locals_ssa_data_node(struct ssa_data_node * node) {
    struct u8_set used_locals_set;
    init_u8_set(&used_locals_set);

    for_each_ssa_data_node(node, NULL, &used_locals_set, SSA_DATA_NODE_OPT_NONE, &get_used_locals_consumer);

    return used_locals_set;
}

void for_each_ssa_data_node(struct ssa_data_node * node, void ** parent_ptr, void * extra, long options, ssa_data_node_consumer_t consumer) {
    for_each_ssa_data_node_recursive(NULL, parent_ptr, node, extra, options, consumer);
}

static void for_each_ssa_data_node_recursive(
        struct ssa_data_node * parent_current,
        void ** parent_current_ptr,
        struct ssa_data_node * current_node,
        void * extra,
        long options,
        ssa_data_node_consumer_t consumer
) {
    if(IS_FLAG_SET(options, SSA_DATA_NODE_OPT_PRE_ORDER)){
        consumer(parent_current, parent_current_ptr, current_node, extra);
    }

    switch (current_node->type) {
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) current_node;
            for(int i = 0; i < init_struct->definition->n_fields; i++) {
                if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_NOT_RECURSIVE)) {
                    consumer(current_node, (void **) &init_struct->fields_nodes[i], init_struct->fields_nodes[i], extra);
                } else {
                    for_each_ssa_data_node_recursive(current_node, (void **) &init_struct->fields_nodes[i], init_struct->fields_nodes[i], extra, options, consumer);
                }
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) current_node;
            if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_NOT_RECURSIVE)) {
                consumer(current_node, (void **) &get_struct_field->instance_node, get_struct_field->instance_node, extra);
            } else {
                for_each_ssa_data_node_recursive(current_node, (void **) &get_struct_field->instance_node, get_struct_field->instance_node, extra, options, consumer);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current_node;
            if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_NOT_RECURSIVE)) {
                consumer(current_node, (void **) &binary_node->left, binary_node->left, extra);
                consumer(current_node, (void **) &binary_node->right, binary_node->right, extra);
            } else {
                for_each_ssa_data_node_recursive(current_node, (void **) &binary_node->left, binary_node->left, extra, options, consumer);
                for_each_ssa_data_node_recursive(current_node, (void **) &binary_node->right, binary_node->right, extra, options, consumer);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) current_node;
            if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_NOT_RECURSIVE)) {
                consumer(current_node, (void **) &unary->operand, unary->operand, extra);
            } else {
                for_each_ssa_data_node_recursive(current_node, (void **) &unary->operand, unary->operand, extra, options, consumer);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call_node = (struct ssa_data_function_call_node *) current_node;
            if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_NOT_RECURSIVE)) {
                consumer(current_node, (void **) &call_node->function, call_node->function, extra);
                for (int i = 0; i < call_node->n_arguments; i++) {
                    consumer(current_node, (void **) &call_node->arguments[i], call_node->arguments[i], extra);
                }
            } else {
                for_each_ssa_data_node_recursive(current_node, (void **) &call_node->function, call_node->function, extra, options, consumer);
                for (int i = 0; i < call_node->n_arguments; i++) {
                    for_each_ssa_data_node_recursive(current_node, (void **) &call_node->arguments[i], call_node->arguments[i], extra, options, consumer);
                }
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) current_node;
            if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_NOT_RECURSIVE)) {
                consumer(current_node, (void **) &get_array_element->instance, get_array_element->instance, extra);
            } else {
                for_each_ssa_data_node_recursive(current_node, (void **) &get_array_element->instance, get_array_element->instance, extra, options, consumer);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) current_node;
            if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_NOT_RECURSIVE)) {
                for(int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++){
                    consumer(current_node, (void **) &init_array->elememnts_node[i], init_array->elememnts_node[i], extra);
                }
            } else {
                for(int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++){
                    for_each_ssa_data_node_recursive(current_node, (void **) &init_array->elememnts_node[i], init_array->elememnts_node[i], extra, options, consumer);
                }
            }
            break;
        }
        default:
            break;
    }

    if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_POST_ORDER)) {
        consumer(parent_current, parent_current_ptr, current_node, extra);
    }
}

struct ssa_data_constant_node * create_ssa_const_node(
        lox_value_t constant_value,
        struct bytecode_list * bytecode,
        struct lox_allocator * allocator
) {
    struct ssa_data_constant_node * constant_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_CONSTANT, struct ssa_data_constant_node, bytecode, allocator
    );

    constant_node->constant_lox_value = constant_value;

    switch (get_lox_type(constant_value)) {
        case VAL_BOOL: {
            constant_node->value_as.boolean = AS_BOOL(constant_value);
            constant_node->data.produced_type = PROFILE_DATA_TYPE_BOOLEAN;
            break;
        }
        case VAL_NIL: {
            constant_node->value_as.nil = NULL;
            constant_node->data.produced_type = PROFILE_DATA_TYPE_NIL;
            break;
        }
        case VAL_NUMBER: {
            if(has_decimals(AS_NUMBER(constant_value))){
                constant_node->value_as.f64 = AS_NUMBER(constant_value);
                constant_node->data.produced_type = PROFILE_DATA_TYPE_F64;
            } else {
                constant_node->value_as.i64 = (int64_t) constant_value;
                constant_node->data.produced_type = PROFILE_DATA_TYPE_I64;
            }
            break;
        }
        case VAL_OBJ: {
            if(IS_STRING(constant_value)){
                constant_node->value_as.string = AS_STRING_OBJECT(constant_value);
                constant_node->data.produced_type = PROFILE_DATA_TYPE_STRING;
            } else {
                constant_node->value_as.object = AS_OBJECT(constant_value);
                constant_node->data.produced_type = PROFILE_DATA_TYPE_OBJECT;
            }
            break;
        }
    }

    return constant_node;
}

static void free_ssa_data_node_consumer(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * current_node,
        void * ___
) {
    NATIVE_LOX_FREE(current_node);
}

void free_ssa_data_node(struct ssa_data_node * node_to_free) {
    for_each_ssa_data_node(node_to_free, NULL, NULL, SSA_DATA_NODE_OPT_NONE, free_ssa_data_node_consumer);
}

uint64_t mix_hash_not_commutative(uint64_t a, uint64_t b) {
    return a ^ b;
}

uint64_t mix_hash_commutative(uint64_t a, uint64_t b) {
    return a ^ (b * 0x9e3779b97f4a7c15);
}

uint64_t hash_ssa_data_node(struct ssa_data_node * node) {
    switch (node->type) {
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            return GET_CONST_VALUE_SSA_NODE(node);
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) node;
            return get_local->local_number;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            struct ssa_data_get_global_node * get_global = (struct ssa_data_get_global_node *) node;
            uint64_t package_name_hash = hash_string(get_global->package->name, strlen(get_global->package->name));
            uint64_t global_name_hash = get_global->name->hash;
            return mix_hash_not_commutative(package_name_hash, global_name_hash);
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            uint64_t hash = 0;
            FOR_EACH_VERSION_IN_PHI_NODE((struct ssa_data_phi_node *) node, current_ssa_name) {
                hash = mix_hash_commutative(hash, current_ssa_name.u16);
            }
            return hash;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) node;
            return mix_hash_not_commutative(0, get_ssa_name->ssa_name.u16);
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call_node = (struct ssa_data_function_call_node *) node;
            uint64_t hash = hash_ssa_data_node(call_node->function);
            for(int i = 0; i < call_node->n_arguments; i++){
                hash = mix_hash_not_commutative(hash, hash_ssa_data_node(call_node->arguments[i]));
            }
            return hash;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) node;
            uint64_t right_hash = hash_ssa_data_node(binary->right);
            uint64_t left_hash = hash_ssa_data_node(binary->left);

            if (is_commutative_bytecode_instruction(binary->operand)) {
                return mix_hash_commutative(left_hash, right_hash);
            } else {
                return mix_hash_not_commutative(left_hash, right_hash);
            }
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) node;
            uint64_t unary_operand_hash = hash_ssa_data_node(unary->operand);
            return mix_hash_not_commutative(unary_operand_hash, unary->operator_type);
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) node;
            uint64_t instance_hash = hash_ssa_data_node(get_struct_field->instance_node);
            uint64_t field_name_hash = get_struct_field->field_name->hash;
            return mix_hash_not_commutative(instance_hash, field_name_hash);
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) node;
            uint64_t hash = init_struct->definition->name->hash;
            for(int i = 0; i < init_struct->definition->n_fields; i++){
                hash = mix_hash_not_commutative(hash, hash_ssa_data_node(init_struct->fields_nodes[i]));
            }
            return hash;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_arr_ele = (struct ssa_data_get_array_element_node *) node;
            uint64_t array_instance_hash = hash_ssa_data_node(get_arr_ele->instance);
            return mix_hash_not_commutative(array_instance_hash, get_arr_ele->index);
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) node;
            uint64_t hash = init_array->n_elements;
            for (int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++) {
                hash = mix_hash_not_commutative(hash, hash_ssa_data_node(init_array->elememnts_node[i]));
            }
            return hash;
        }
        default:
            runtime_panic("Illegal code path in hash_expression(struct ssa_data_node *)");
    }
}