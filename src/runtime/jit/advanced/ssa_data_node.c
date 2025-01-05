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
            return false;
    }
}

bool get_used_ssa_names_ssa_data_node_consumer(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * data_node,
        void * extra
) {
    struct u64_set * used_ssa_names = extra;

    if (data_node->type == SSA_DATA_NODE_TYPE_GET_SSA_NAME) {
        struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) data_node;
        add_u64_set(used_ssa_names, get_ssa_name->ssa_name.u16);
    }
    if(data_node->type == SSA_DATA_NODE_TYPE_PHI){
        FOR_EACH_SSA_NAME_IN_PHI_NODE((struct ssa_data_phi_node *) data_node, current_phi_ssa_name) {
            add_u64_set(used_ssa_names, current_phi_ssa_name.u16);
        }
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
            return a_unary->operator == b_unary->operator &&
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
            if (a_binary->operator != b_binary->operator) {
                return false;
            }
            bool is_commutative = is_commutative_associative_bytecode_instruction(a_binary->operator);
            if (!is_commutative) {
                return is_eq_ssa_data_node(a_binary->left, b_binary->left, allocator) && is_eq_ssa_data_node(a_binary->right, b_binary->right, allocator);
            }
            //Check for commuativiy equivalence
            if((is_eq_ssa_data_node(a_binary->left, b_binary->left, allocator) && is_eq_ssa_data_node(a_binary->right, b_binary->right, allocator)) ||
               (is_eq_ssa_data_node(a_binary->left, b_binary->right, allocator) && is_eq_ssa_data_node(a_binary->left, b_binary->right, allocator))) {
                return true;
            }
            if(!uses_same_binary_operation(a, a_binary->operator) || !uses_same_binary_operation(b, b_binary->operator)){
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

    struct u64_set children_ptr_set = get_children_ssa_data_node(current_node, NATIVE_LOX_ALLOCATOR());
    FOR_EACH_U64_SET_VALUE(children_ptr_set, children_ptr_u64) {
        struct ssa_data_node ** children_ptr = (struct ssa_data_node **) children_ptr_u64;
        for_each_ssa_data_node_recursive(current_node, (void **) children_ptr, *children_ptr, extra, options, consumer);
    }

    free_u64_set(&children_ptr_set);

    if (IS_FLAG_SET(options, SSA_DATA_NODE_OPT_POST_ORDER)) {
        consumer(parent_current, parent_current_ptr, current_node, extra);
    }

    return true;
}

struct ssa_data_constant_node * create_ssa_const_node(
        uint64_t constant_value,
        ssa_type_t type,
        struct bytecode_list * bytecode,
        struct lox_allocator * allocator
) {
    struct ssa_data_constant_node * constant_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_CONSTANT, struct ssa_data_constant_node, bytecode, allocator
    );

    constant_node->data.produced_type = CREATE_SSA_TYPE(type, allocator);
    constant_node->value = constant_value;

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
            FOR_EACH_SSA_NAME_IN_PHI_NODE((struct ssa_data_phi_node *) node, current_ssa_name) {
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

            uint64_t operand_hash = is_commutative_associative_bytecode_instruction(binary->operator) ?
                    mix_hash_commutative(left_hash, right_hash) :
                    mix_hash_not_commutative(left_hash, right_hash);
            return mix_hash_not_commutative(operand_hash, binary->operator);
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) node;
            uint64_t unary_operand_hash = hash_ssa_data_node(unary->operand);
            return mix_hash_not_commutative(unary_operand_hash, unary->operator);
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
        if(binary->operator != consumer_struct->expected_operation){
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

struct ssa_data_guard_node * create_from_profile_ssa_data_guard_node(
        struct type_profile_data type_profile,
        struct ssa_data_node * source,
        struct lox_allocator * allocator
) {
    profile_data_type_t profiled_type = get_type_by_type_profile_data(type_profile);

    struct ssa_data_guard_node * guard_node = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_GUARD, struct ssa_data_guard_node, NULL,
            allocator);
    guard_node->guard.value = source;

    if(profiled_type == PROFILE_DATA_TYPE_STRUCT_INSTANCE && type_profile.invalid_struct_definition){
        guard_node->guard.type = SSA_GUARD_STRUCT_DEFINITION_TYPE_CHECK;
        guard_node->guard.value_to_compare.struct_definition = type_profile.struct_definition;
    } else {
        guard_node->guard.type = SSA_GUARD_TYPE_CHECK;
        guard_node->guard.value_to_compare.type = profiled_type_to_ssa_type(profiled_type);
    }

    return guard_node;
}

struct u64_set get_children_ssa_data_node(struct ssa_data_node * node, struct lox_allocator * allocator) {
    struct u64_set children;
    init_u64_set(&children, allocator);

    switch (node->type) {
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call_node = (struct ssa_data_function_call_node *) node;
            for(int i = 0 ; i < call_node->n_arguments; i++){
                add_u64_set(&children, (uint64_t) &call_node->arguments[i]);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) node;
            add_u64_set(&children, (uint64_t) &binary->left);
            add_u64_set(&children, (uint64_t) &binary->right);
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) node;
            add_u64_set(&children, (uint64_t) &unary->operand);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) node;
            add_u64_set(&children, (uint64_t) &get_struct_field->instance_node);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) node;
            for(int i = 0; i < init_struct->definition->n_fields; i++){
                add_u64_set(&children, (uint64_t) &init_struct->fields_nodes[i]);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_arr_element = (struct ssa_data_get_array_element_node *) node;
            add_u64_set(&children, (uint64_t) &get_arr_element->instance);
            add_u64_set(&children, (uint64_t) &get_arr_element->index);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) node;
            for(int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++){
                add_u64_set(&children, (uint64_t) &init_array->elememnts_node[i]);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH: {
            struct ssa_data_get_array_length * get_arr_len = (struct ssa_data_get_array_length *) node;
            add_u64_set(&children, (uint64_t) &get_arr_len->instance);
            break;
        }
        case SSA_DATA_NODE_TYPE_GUARD: {
            struct ssa_data_guard_node * guard = (struct ssa_data_guard_node *) node;
            add_u64_set(&children, (uint64_t) &guard->guard.value);
            break;
        }
        case SSA_DATA_NODE_TYPE_UNBOX: {
            struct ssa_data_node_unbox * unbox = (struct ssa_data_node_unbox *) node;
            add_u64_set(&children, (uint64_t) &unbox->to_unbox);
            break;
        }
        case SSA_DATA_NODE_TYPE_BOX: {
            struct ssa_data_node_box * box = (struct ssa_data_node_box *) node;
            add_u64_set(&children, (uint64_t) &box->to_box);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        case SSA_DATA_NODE_TYPE_CONSTANT:
        case SSA_DATA_NODE_TYPE_PHI:
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            break;
        }
    }

    return children;
}

void unbox_const_ssa_data_node(struct ssa_data_constant_node * const_node) {
    if (is_lox_ssa_type(const_node->data.produced_type->type)) {
        const_node->data.produced_type->type = lox_type_to_native_ssa_type(const_node->data.produced_type->type);
        const_node->value = lox_to_native_type((lox_value_t) const_node->value);
    }
}

bool is_escaped_ssa_data_node(struct ssa_data_node * data_node) {
    switch (data_node->type) {
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) data_node;
            return get_struct_field->escapes;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) data_node;
            return init_struct->escapes;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_arr_ele = (struct ssa_data_get_array_element_node *) data_node;
            return get_arr_ele->escapes;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) data_node;
            return init_array->escapes;
        }
        default:
            return false;
    }
}

void mark_as_escaped_ssa_data_node(struct ssa_data_node * data_node) {
    switch (data_node->type) {
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) data_node;
            get_struct_field->escapes = true;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) data_node;
            init_struct->escapes = true;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_arr_ele = (struct ssa_data_get_array_element_node *) data_node;
            get_arr_ele->escapes = true;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) data_node;
            init_array->escapes = true;
        }
    }
}