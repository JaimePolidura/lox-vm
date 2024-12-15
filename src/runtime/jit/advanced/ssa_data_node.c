#include "ssa_data_node.h"

extern void runtime_panic(char * format, ...);
static bool uses_same_binary_operation(struct ssa_data_node *, bytecode_t);
static struct u64_set flat_out_binary_operand_nodes(struct ssa_data_node *node, struct lox_allocator *);
static bool check_equivalence_flatted_out(struct u64_set, struct u64_set, struct lox_allocator *);

static bool for_each_ssa_data_node_recursive(
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

bool is_terminator_ssa_data_node(struct ssa_data_node * node) {
    switch (node->type) {
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        case SSA_DATA_NODE_TYPE_PHI:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CONSTANT:
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME:
            return true;
        case SSA_DATA_NODE_TYPE_GUARD:
        case SSA_DATA_NODE_TYPE_CALL:
        case SSA_DATA_NODE_TYPE_BINARY:
        case SSA_DATA_NODE_TYPE_UNARY:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH:
        case SSA_DATA_NODE_UNBOX:
        case SSA_DATA_NODE_BOX:
            return false;
    }
}

bool get_used_ssa_names_ssa_data_node_consumer(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * data_node,
        void * extra
) {
    if (data_node->type == SSA_DATA_NODE_TYPE_GET_SSA_NAME) {
        struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) data_node;
        struct u64_set * used_ssa_names = extra;
        add_u64_set(used_ssa_names, get_ssa_name->ssa_name.u16);
    }

    return true;
}

struct u64_set get_used_ssa_names_ssa_data_node(struct ssa_data_node * data_node, struct lox_allocator * allocator) {
    struct u64_set used_ssa_names;
    init_u64_set(&used_ssa_names, allocator);

    for_each_ssa_data_node(
            data_node,
            NULL,
            &used_ssa_names,
            SSA_DATA_NODE_OPT_POST_ORDER,
            get_used_ssa_names_ssa_data_node_consumer
    );

    free_u64_set(&used_ssa_names);

    return used_ssa_names;
}

bool get_used_locals_consumer(struct ssa_data_node * _, void ** __, struct ssa_data_node * current_node, void * extra) {
    struct u8_set * used_locals_set = (struct u8_set *) extra;

    if(current_node->type == SSA_DATA_NODE_TYPE_GET_LOCAL){
        struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) current_node;
        add_u8_set(used_locals_set, get_local->local_number);
    } else if(current_node->type == SSA_DATA_NODE_TYPE_PHI){
        struct ssa_data_phi_node * phi = (struct ssa_data_phi_node *) current_node;
        FOR_EACH_U64_SET_VALUE(phi->ssa_versions, phi_version) {
            add_u8_set(used_locals_set, phi_version);
        }
    }

    return true;
}

struct u8_set get_used_locals_ssa_data_node(struct ssa_data_node * node) {
    struct u8_set used_locals_set;
    init_u8_set(&used_locals_set);

    for_each_ssa_data_node(
            node,
            NULL,
            &used_locals_set,
            SSA_DATA_NODE_OPT_POST_ORDER,
            &get_used_locals_consumer
    );

    return used_locals_set;
}

bool is_eq_ssa_data_node(struct ssa_data_node * a, struct ssa_data_node * b, struct lox_allocator * allocator) {
    if(a->type != b->type){
        return false;
    }

    //At this point a & b have the same type.
    switch (a->type) {
        case SSA_DATA_NODE_TYPE_GUARD: {
            struct ssa_data_guard_node * a_guard = (struct ssa_data_guard_node *) a;
            struct ssa_data_guard_node * b_guard = (struct ssa_data_guard_node *) b;
            return is_eq_ssa_data_node(a_guard->guard.value, b_guard->guard.value, allocator);
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            return GET_CONST_VALUE_SSA_NODE(a) == GET_CONST_VALUE_SSA_NODE(b);
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_data_phi_node * a_phi = (struct ssa_data_phi_node *) a;
            struct ssa_data_phi_node * b_phi = (struct ssa_data_phi_node *) b;
            return a_phi->local_number == b_phi->local_number &&
                size_u64_set(a_phi->ssa_versions) == size_u64_set(b_phi->ssa_versions) &&
                is_subset_u64_set(a_phi->ssa_versions, b_phi->ssa_versions);
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * a_unary = (struct ssa_data_unary_node *) a;
            struct ssa_data_unary_node * b_unary = (struct ssa_data_unary_node *) b;
            return a_unary->operator_type == b_unary->operator_type &&
                    is_eq_ssa_data_node(a_unary->operand, b_unary->operand, allocator);
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * a_get_ssa_name = (struct ssa_data_get_ssa_name_node *) a;
            struct ssa_data_get_ssa_name_node * b_get_ssa_name = (struct ssa_data_get_ssa_name_node *) b;
            return a_get_ssa_name->ssa_name.value.local_number == b_get_ssa_name->ssa_name.value.local_number &&
                    a_get_ssa_name->ssa_name.value.version == b_get_ssa_name->ssa_name.value.version;
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * a_get_local = (struct ssa_data_get_local_node *) a;
            struct ssa_data_get_local_node * b_get_local = (struct ssa_data_get_local_node *) b;
            return a_get_local->local_number == b_get_local->local_number;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH: {
            struct ssa_data_get_array_length * a_get_array_length = (struct ssa_data_get_array_length *) a;
            struct ssa_data_get_array_length * b_get_array_length = (struct ssa_data_get_array_length *) b;
            return is_eq_ssa_data_node(a_get_array_length->instance, b_get_array_length->instance, allocator);
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            struct ssa_data_get_global_node * a_get_glocal = (struct ssa_data_get_global_node *) a;
            struct ssa_data_get_global_node * b_get_glocal = (struct ssa_data_get_global_node *) b;
            return a_get_glocal->package == b_get_glocal->package &&
                a_get_glocal->name->length == b_get_glocal->name->length &&
                string_equals_ignore_case(a_get_glocal->name->chars, b_get_glocal->name->chars, b_get_glocal->name->length);
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * a_call = (struct ssa_data_function_call_node *) a;
            struct ssa_data_function_call_node * b_call = (struct ssa_data_function_call_node *) b;
            if(a_call->is_native != b_call->is_native || (a_call->lox_function.function != b_call->lox_function.function &&
                a_call->native_function != b_call->native_function)) {
                return false;
            }
            for(int i = 0; i < a_call->n_arguments; i++){
                if(!is_eq_ssa_data_node(a_call->arguments[i], b_call->arguments[i], allocator)){
                    return false;
                }
            }
            return true;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * a_get_field = (struct ssa_data_get_struct_field_node *) a;
            struct ssa_data_get_struct_field_node * b_get_field = (struct ssa_data_get_struct_field_node *) b;
            return a_get_field->field_name->length == b_get_field->field_name->length &&
                    string_equals_ignore_case(a_get_field->field_name->chars, b_get_field->field_name->chars, a_get_field->field_name->length) &&
                    is_eq_ssa_data_node(a_get_field->instance_node, b_get_field->instance_node, allocator);
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * a_init_struct = (struct ssa_data_initialize_struct_node *) a;
            struct ssa_data_initialize_struct_node * b_init_struct = (struct ssa_data_initialize_struct_node *) b;
            if(a_init_struct->definition != b_init_struct->definition){
                return false;
            }
            for(int i = 0; i < a_init_struct->definition->n_fields; i++){
                if(!is_eq_ssa_data_node(a_init_struct->fields_nodes[i], b_init_struct->fields_nodes[i], allocator)){
                    return false;
                }
            }
            return true;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * a_get_array_ele = (struct ssa_data_get_array_element_node *) a;
            struct ssa_data_get_array_element_node * b_get_array_ele = (struct ssa_data_get_array_element_node *) b;
            return is_eq_ssa_data_node(a_get_array_ele->index, b_get_array_ele->index, allocator) &&
                   is_eq_ssa_data_node(a_get_array_ele->instance, b_get_array_ele->instance, allocator);
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * a_init_array = (struct ssa_data_initialize_array_node *) a;
            struct ssa_data_initialize_array_node * b_init_array = (struct ssa_data_initialize_array_node *) b;
            if (a_init_array->empty_initialization != b_init_array->empty_initialization ||
                a_init_array->n_elements != b_init_array->n_elements) {
                return false;
            }

            for(int i = 0; i < a_init_array->n_elements && !a_init_array->empty_initialization; i++){
                if(is_eq_ssa_data_node(a_init_array->elememnts_node[i], b_init_array->elememnts_node[i], allocator)){
                    return false;
                }
            }

            return true;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * a_binary = (struct ssa_data_binary_node *) a;
            struct ssa_data_binary_node * b_binary = (struct ssa_data_binary_node *) b;
            if (a_binary->operand != b_binary->operand) {
                return false;
            }
            bool is_commutative = is_commutative_associative_bytecode_instruction(a_binary->operand);
            if (!is_commutative) {
                return is_eq_ssa_data_node(a_binary->left, b_binary->left, allocator) && is_eq_ssa_data_node(a_binary->right, b_binary->right, allocator);
            }
            //Check for commuativiy equivalence
            if((is_eq_ssa_data_node(a_binary->left, b_binary->left, allocator) && is_eq_ssa_data_node(a_binary->right, b_binary->right, allocator)) ||
               (is_eq_ssa_data_node(a_binary->left, b_binary->right, allocator) && is_eq_ssa_data_node(a_binary->left, b_binary->right, allocator))) {
                return true;
            }
            if(!uses_same_binary_operation(a, a_binary->operand) || !uses_same_binary_operation(b, b_binary->operand)){
                return false;
            }
            //Check for associative equivalence
            struct u64_set rigth_operands_flatted_out = flat_out_binary_operand_nodes(b, allocator);
            struct u64_set left_operands_flatted_out = flat_out_binary_operand_nodes(a, allocator);
            return check_equivalence_flatted_out(left_operands_flatted_out, rigth_operands_flatted_out, allocator);
        }
    }
}

void for_each_ssa_data_node(
        struct ssa_data_node * node,
        void ** parent_ptr,
        void * extra,
        long options,
        ssa_data_node_consumer_t consumer
) {
    for_each_ssa_data_node_recursive(NULL, parent_ptr, node, extra, options, consumer);
}

static bool for_each_ssa_data_node_recursive(
        struct ssa_data_node * parent_current,
        void ** parent_current_ptr,
        struct ssa_data_node * current_node,
        void * extra,
        long options,
        ssa_data_node_consumer_t consumer
) {
    if(IS_FLAG_SET(options, SSA_DATA_NODE_OPT_NOT_TERMINATORS) && is_terminator_ssa_data_node(current_node)){
        return true;
    }
    if(IS_FLAG_SET(options, SSA_DATA_NODE_OPT_ONLY_TERMINATORS) && is_terminator_ssa_data_node(current_node)){
        consumer(parent_current, parent_current_ptr, current_node, extra);
        return true;
    }
    if(IS_FLAG_SET(options, SSA_DATA_NODE_OPT_PRE_ORDER)){
        //Keep scanning but not from this node anymmore
        if(!consumer(parent_current, parent_current_ptr, current_node, extra)) {
            return true;
        }
    }

    switch (current_node->type) {
        case SSA_DATA_NODE_TYPE_GUARD: {
            struct ssa_data_guard_node * guard_node = (struct ssa_data_guard_node * ) current_node;
            for_each_ssa_data_node_recursive(current_node, (void **) &guard_node->guard.value, guard_node->guard.value, extra, options, consumer);
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) current_node;
            for(int i = 0; i < init_struct->definition->n_fields; i++) {
                for_each_ssa_data_node_recursive(current_node, (void **) &init_struct->fields_nodes[i], init_struct->fields_nodes[i], extra, options, consumer);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) current_node;
            for_each_ssa_data_node_recursive(current_node, (void **) &get_struct_field->instance_node, get_struct_field->instance_node, extra, options, consumer);
            break;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current_node;
            for_each_ssa_data_node_recursive(current_node, (void **) &binary_node->right, binary_node->right, extra, options, consumer);
            for_each_ssa_data_node_recursive(current_node, (void **) &binary_node->left, binary_node->left, extra, options, consumer);
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) current_node;
            for_each_ssa_data_node_recursive(current_node, (void **) &unary->operand, unary->operand, extra, options, consumer);
            break;
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call_node = (struct ssa_data_function_call_node *) current_node;
            for (int i = 0; i < call_node->n_arguments; i++) {
                for_each_ssa_data_node_recursive(current_node, (void **) &call_node->arguments[i], call_node->arguments[i], extra, options, consumer);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH: {
            struct ssa_data_get_array_length * get_array_element = (struct ssa_data_get_array_length *) current_node;
            for_each_ssa_data_node_recursive(current_node, (void **) &get_array_element->instance, get_array_element->instance, extra, options, consumer);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) current_node;
            for_each_ssa_data_node_recursive(current_node, (void **) &get_array_element->instance, get_array_element->instance, extra, options, consumer);
            for_each_ssa_data_node_recursive(current_node, (void **) &get_array_element->index, get_array_element->index, extra, options, consumer);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) current_node;
            for(int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++){
                for_each_ssa_data_node_recursive(current_node, (void **) &init_array->elememnts_node[i],
                                                 init_array->elememnts_node[i], extra, options, consumer);
            }
            break;
        }
        default:
            break;
    }

    if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_POST_ORDER)) {
        consumer(parent_current, parent_current_ptr, current_node, extra);
    }

    return true;
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
            }
            break;
        }
    }

    return constant_node;
}

static bool free_ssa_data_node_consumer(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * current_node,
        void * ___
) {
    NATIVE_LOX_FREE(current_node);
    return true;
}

void free_ssa_data_node(struct ssa_data_node * node_to_free) {
    for_each_ssa_data_node(
            node_to_free,
            NULL,
            NULL,
            SSA_DATA_NODE_OPT_POST_ORDER,
            free_ssa_data_node_consumer
    );
}

uint64_t mix_hash_not_commutative(uint64_t a, uint64_t b) {
    return a ^ (b * 0x9e3779b97f4a7c15);
}

uint64_t mix_hash_commutative(uint64_t a, uint64_t b) {
    return a ^ b;
}

uint64_t hash_ssa_data_node(struct ssa_data_node * node) {
    switch (node->type) {
        case SSA_DATA_NODE_TYPE_GUARD: {
            struct ssa_data_guard_node * guard = (struct ssa_data_guard_node *) node;
            return hash_ssa_data_node(guard->guard.value);
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            return GET_CONST_VALUE_SSA_NODE(node);
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH: {
            struct ssa_data_get_array_length * get_array_length = (struct ssa_data_get_array_length *) node;
            return mix_hash_commutative(SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH, hash_ssa_data_node(get_array_length->instance));
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) node;
            return mix_hash_commutative(SSA_DATA_NODE_TYPE_GET_LOCAL, get_local->local_number);
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
            uint64_t hash = call_node->is_native ? (uint64_t) call_node->lox_function.function : (uint64_t) call_node->native_function;
            for(int i = 0; i < call_node->n_arguments; i++){
                hash = mix_hash_not_commutative(hash, hash_ssa_data_node(call_node->arguments[i]));
            }
            return hash;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) node;
            uint64_t right_hash = hash_ssa_data_node(binary->right);
            uint64_t left_hash = hash_ssa_data_node(binary->left);

            uint64_t operand_hash = is_commutative_associative_bytecode_instruction(binary->operand) ?
                    mix_hash_commutative(left_hash, right_hash) :
                    mix_hash_not_commutative(left_hash, right_hash);
            return mix_hash_not_commutative(operand_hash, binary->operand);
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
            uint64_t index_instance_hash = hash_ssa_data_node(get_arr_ele->index);
            return mix_hash_not_commutative(array_instance_hash, index_instance_hash);
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

struct uses_same_binary_operation_consumer_struct {
    bool uses_same_op;
    bytecode_t expected_operation;
};

bool uses_same_binary_operation_consumer(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * child,
        void * extra
) {
    struct uses_same_binary_operation_consumer_struct * consumer_struct = extra;
    if (child->type == SSA_DATA_NODE_TYPE_BINARY && consumer_struct->uses_same_op) {
        struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) child;
        if(binary->operand != consumer_struct->expected_operation){
            consumer_struct->uses_same_op = false;
        }
    }

    return true;
}

static bool uses_same_binary_operation(struct ssa_data_node * start_node, bytecode_t operation) {
    struct uses_same_binary_operation_consumer_struct consumer_struct = (struct uses_same_binary_operation_consumer_struct) {
        .expected_operation = operation,
        .uses_same_op = true,
    };

    for_each_ssa_data_node(
            start_node,
            NULL,
            &consumer_struct,
            SSA_DATA_NODE_OPT_POST_ORDER,
            uses_same_binary_operation_consumer
    );

    return consumer_struct.uses_same_op;
}

static bool flat_out_binary_operand_nodes_consumer(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * child,
        void * extra
) {
    struct u64_set * operands = extra;
    if (child->type != SSA_DATA_NODE_TYPE_BINARY) {
        add_u64_set(operands, (uint64_t) child);
    }
    return true;
}

static struct u64_set flat_out_binary_operand_nodes(struct ssa_data_node * node, struct lox_allocator * allocator) {
    struct u64_set operands;
    init_u64_set(&operands, allocator);

    for_each_ssa_data_node(
            node,
            NULL,
            &operands,
            SSA_DATA_NODE_OPT_POST_ORDER,
            &flat_out_binary_operand_nodes_consumer
    );

    return operands;
}

static bool check_equivalence_flatted_out(struct u64_set left, struct u64_set right, struct lox_allocator * allocator) {
    FOR_EACH_U64_SET_VALUE(left, current_left_data_node_u64_ptr) {
        struct ssa_data_node * current_left_data_node = (struct ssa_data_node *) current_left_data_node_u64_ptr;
        bool some_match_in_right_found = false;

        FOR_EACH_U64_SET_VALUE(right, current_right_data_node_u64_ptr) {
            struct ssa_data_node * current_right_data_node = (struct ssa_data_node *) current_left_data_node_u64_ptr;
            if(is_eq_ssa_data_node(current_left_data_node, current_right_data_node, allocator)){
                some_match_in_right_found = true;
                break;
            }
        }

        if(some_match_in_right_found){
            return true;
        }
    }

    return false;
}