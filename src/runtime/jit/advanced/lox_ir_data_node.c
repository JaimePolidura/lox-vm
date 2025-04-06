#include "lox_ir_data_node.h"

extern void runtime_panic(char * format, ...);
static bool uses_same_binary_operation(struct lox_ir_data_node *, bytecode_t);
static struct u64_set flat_out_binary_operand_nodes(struct lox_ir_data_node *node, struct lox_allocator *);
static bool check_equivalence_flatted_out(struct u64_set, struct u64_set, struct lox_allocator *);

static bool for_each_lox_ir_data_node_recursive(
        struct lox_ir_data_node * parent_current,
        void ** parent_current_ptr,
        struct lox_ir_data_node * current_node,
        void * extra,
        long options,
        lox_ir_data_node_consumer_t consumer
);

void * allocate_lox_ir_data_node(
        lox_ir_data_node_type type,
        size_t struct_size_bytes,
        struct bytecode_list * bytecode,
        struct lox_allocator * allocator
) {
    struct lox_ir_data_node * data_node = LOX_MALLOC(allocator, struct_size_bytes);
    memset(data_node, 0, struct_size_bytes);
    data_node->original_bytecode = bytecode;
    data_node->type = type;

    if (data_node->type == LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT) {
        ((struct lox_ir_data_get_array_element_node *) data_node)->requires_range_check = true;
    }

    return data_node;
}

bool is_terminator_lox_ir_data_node(struct lox_ir_data_node * node) {
    switch (node->type) {
        case LOX_IR_DATA_NODE_GET_LOCAL:
        case LOX_IR_DATA_NODE_PHI:
        case LOX_IR_DATA_NODE_GET_GLOBAL:
        case LOX_IR_DATA_NODE_CONSTANT:
        case LOX_IR_DATA_NODE_GET_SSA_NAME:
        case LOX_IR_DATA_NODE_GET_V_REGISTER:
            return true;
        case LOX_IR_DATA_NODE_GUARD:
        case LOX_IR_DATA_NODE_CALL:
        case LOX_IR_DATA_NODE_BINARY:
        case LOX_IR_DATA_NODE_UNARY:
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD:
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT:
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT:
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY:
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH:
        case LOX_IR_DATA_NODE_CAST:
            return false;
    }
}

bool get_used_v_registers_ox_ir_data_node_consumer(
        struct lox_ir_data_node * _,
        void ** __,
        struct lox_ir_data_node * data_node,
        void * extra
) {
    struct u64_set * used_v_registers = extra;
    if(data_node->type == LOX_IR_DATA_NODE_GET_V_REGISTER){
        struct lox_ir_data_get_v_register_node * get_v_reg = (struct lox_ir_data_get_v_register_node *) data_node;
        add_u64_set(used_v_registers, get_v_reg->v_register.number);
    }
    return true;
}

struct u64_set get_used_v_registers_lox_ir_data_node(struct lox_ir_data_node * data_node, struct lox_allocator * allocator) {
    struct u64_set used_v_registers;
    init_u64_set(&used_v_registers, allocator);

    for_each_lox_ir_data_node(
            data_node,
            NULL,
            &used_v_registers,
            LOX_IR_DATA_NODE_OPT_POST_ORDER,
            get_used_v_registers_ox_ir_data_node_consumer
    );

    return used_v_registers;
}

bool get_used_ssa_names_lox_ir_data_node_consumer(
        struct lox_ir_data_node * _,
        void ** __,
        struct lox_ir_data_node * data_node,
        void * extra
) {
    struct u64_set * used_ssa_names = extra;

    if (data_node->type == LOX_IR_DATA_NODE_GET_SSA_NAME) {
        struct lox_ir_data_get_ssa_name_node * get_ssa_name = (struct lox_ir_data_get_ssa_name_node *) data_node;
        add_u64_set(used_ssa_names, get_ssa_name->ssa_name.u16);
    }
    if(data_node->type == LOX_IR_DATA_NODE_PHI){
        FOR_EACH_SSA_NAME_IN_PHI_NODE((struct lox_ir_data_phi_node *) data_node, current_phi_ssa_name) {
            add_u64_set(used_ssa_names, current_phi_ssa_name.u16);
        }
    }

    return true;
}

struct u64_set get_used_ssa_names_lox_ir_data_node(struct lox_ir_data_node * data_node, struct lox_allocator * allocator) {
    struct u64_set used_ssa_names;
    init_u64_set(&used_ssa_names, allocator);

    for_each_lox_ir_data_node(
            data_node,
            NULL,
            &used_ssa_names,
            LOX_IR_DATA_NODE_OPT_POST_ORDER,
            get_used_ssa_names_lox_ir_data_node_consumer
    );

    return used_ssa_names;
}

bool get_used_locals_consumer(struct lox_ir_data_node * _, void ** __, struct lox_ir_data_node * current_node, void * extra) {
    struct u8_set * used_locals_set = (struct u8_set *) extra;

    if(current_node->type == LOX_IR_DATA_NODE_GET_LOCAL){
        struct lox_ir_data_get_local_node * get_local = (struct lox_ir_data_get_local_node *) current_node;
        add_u8_set(used_locals_set, get_local->local_number);
    } else if(current_node->type == LOX_IR_DATA_NODE_PHI){
        struct lox_ir_data_phi_node * phi = (struct lox_ir_data_phi_node *) current_node;
        FOR_EACH_U64_SET_VALUE(phi->ssa_versions, uint64_t, phi_version) {
            add_u8_set(used_locals_set, phi_version);
        }
    }

    return true;
}

struct u8_set get_used_locals_lox_ir_data_node(struct lox_ir_data_node * node) {
    struct u8_set used_locals_set;
    init_u8_set(&used_locals_set);

    for_each_lox_ir_data_node(
            node,
            NULL,
            &used_locals_set,
            LOX_IR_DATA_NODE_OPT_POST_ORDER,
            &get_used_locals_consumer
    );

    return used_locals_set;
}

bool is_eq_lox_ir_data_node(struct lox_ir_data_node * a, struct lox_ir_data_node * b, struct lox_allocator * allocator) {
    if(a->type != b->type){
        return false;
    }

    //At this point a & b have the same type.
    switch (a->type) {
        case LOX_IR_DATA_NODE_GET_V_REGISTER: {
            struct lox_ir_data_get_v_register_node * get_v_reg_a = (struct lox_ir_data_get_v_register_node *) a;
            struct lox_ir_data_get_v_register_node * get_v_reg_b = (struct lox_ir_data_get_v_register_node *) b;
            return get_v_reg_a->v_register.number == get_v_reg_b->v_register.number &&
                    get_v_reg_a->v_register.is_float_register == get_v_reg_b->v_register.is_float_register;
        }
        case LOX_IR_DATA_NODE_GUARD: {
            struct lox_ir_data_guard_node * a_guard = (struct lox_ir_data_guard_node *) a;
            struct lox_ir_data_guard_node * b_guard = (struct lox_ir_data_guard_node *) b;
            return is_eq_lox_ir_data_node(a_guard->guard.value, b_guard->guard.value, allocator);
        }
        case LOX_IR_DATA_NODE_CONSTANT: {
            return GET_CONST_VALUE_LOX_IR_NODE(a) == GET_CONST_VALUE_LOX_IR_NODE(b);
        }
        case LOX_IR_DATA_NODE_PHI: {
            struct lox_ir_data_phi_node * a_phi = (struct lox_ir_data_phi_node *) a;
            struct lox_ir_data_phi_node * b_phi = (struct lox_ir_data_phi_node *) b;
            return a_phi->local_number == b_phi->local_number &&
                size_u64_set(a_phi->ssa_versions) == size_u64_set(b_phi->ssa_versions) &&
                is_subset_u64_set(a_phi->ssa_versions, b_phi->ssa_versions);
        }
        case LOX_IR_DATA_NODE_UNARY: {
            struct lox_ir_data_unary_node * a_unary = (struct lox_ir_data_unary_node *) a;
            struct lox_ir_data_unary_node * b_unary = (struct lox_ir_data_unary_node *) b;
            return a_unary->operator == b_unary->operator &&
                    is_eq_lox_ir_data_node(a_unary->operand, b_unary->operand, allocator);
        }
        case LOX_IR_DATA_NODE_GET_SSA_NAME: {
            struct lox_ir_data_get_ssa_name_node * a_get_ssa_name = (struct lox_ir_data_get_ssa_name_node *) a;
            struct lox_ir_data_get_ssa_name_node * b_get_ssa_name = (struct lox_ir_data_get_ssa_name_node *) b;
            return a_get_ssa_name->ssa_name.value.local_number == b_get_ssa_name->ssa_name.value.local_number &&
                    a_get_ssa_name->ssa_name.value.version == b_get_ssa_name->ssa_name.value.version;
        }
        case LOX_IR_DATA_NODE_GET_LOCAL: {
            struct lox_ir_data_get_local_node * a_get_local = (struct lox_ir_data_get_local_node *) a;
            struct lox_ir_data_get_local_node * b_get_local = (struct lox_ir_data_get_local_node *) b;
            return a_get_local->local_number == b_get_local->local_number;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            struct lox_ir_data_get_array_length * a_get_array_length = (struct lox_ir_data_get_array_length *) a;
            struct lox_ir_data_get_array_length * b_get_array_length = (struct lox_ir_data_get_array_length *) b;
            return is_eq_lox_ir_data_node(a_get_array_length->instance, b_get_array_length->instance, allocator);
        }
        case LOX_IR_DATA_NODE_GET_GLOBAL: {
            struct lox_ir_data_get_global_node * a_get_glocal = (struct lox_ir_data_get_global_node *) a;
            struct lox_ir_data_get_global_node * b_get_glocal = (struct lox_ir_data_get_global_node *) b;
            return a_get_glocal->package == b_get_glocal->package &&
                a_get_glocal->name->length == b_get_glocal->name->length &&
                string_equals_ignore_case(a_get_glocal->name->chars, b_get_glocal->name->chars, b_get_glocal->name->length);
        }
        case LOX_IR_DATA_NODE_CALL: {
            struct lox_ir_data_function_call_node * a_call = (struct lox_ir_data_function_call_node *) a;
            struct lox_ir_data_function_call_node * b_call = (struct lox_ir_data_function_call_node *) b;
            if(a_call->is_native != b_call->is_native || (a_call->lox_function.function != b_call->lox_function.function &&
                a_call->native_function != b_call->native_function)) {
                return false;
            }
            for(int i = 0; i < a_call->n_arguments; i++){
                if(!is_eq_lox_ir_data_node(a_call->arguments[i], b_call->arguments[i], allocator)){
                    return false;
                }
            }
            return true;
        }
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD: {
            struct lox_ir_data_get_struct_field_node * a_get_field = (struct lox_ir_data_get_struct_field_node *) a;
            struct lox_ir_data_get_struct_field_node * b_get_field = (struct lox_ir_data_get_struct_field_node *) b;
            return a_get_field->field_name->length == b_get_field->field_name->length &&
                   string_equals_ignore_case(a_get_field->field_name->chars, b_get_field->field_name->chars, a_get_field->field_name->length) &&
                    is_eq_lox_ir_data_node(a_get_field->instance, b_get_field->instance, allocator);
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            struct lox_ir_data_initialize_struct_node * a_init_struct = (struct lox_ir_data_initialize_struct_node *) a;
            struct lox_ir_data_initialize_struct_node * b_init_struct = (struct lox_ir_data_initialize_struct_node *) b;
            if(a_init_struct->definition != b_init_struct->definition){
                return false;
            }
            for(int i = 0; i < a_init_struct->definition->n_fields; i++){
                if(!is_eq_lox_ir_data_node(a_init_struct->fields_nodes[i], b_init_struct->fields_nodes[i], allocator)){
                    return false;
                }
            }
            return true;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT: {
            struct lox_ir_data_get_array_element_node * a_get_array_ele = (struct lox_ir_data_get_array_element_node *) a;
            struct lox_ir_data_get_array_element_node * b_get_array_ele = (struct lox_ir_data_get_array_element_node *) b;
            return is_eq_lox_ir_data_node(a_get_array_ele->index, b_get_array_ele->index, allocator) &&
                   is_eq_lox_ir_data_node(a_get_array_ele->instance, b_get_array_ele->instance, allocator);
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            struct lox_ir_data_initialize_array_node * a_init_array = (struct lox_ir_data_initialize_array_node *) a;
            struct lox_ir_data_initialize_array_node * b_init_array = (struct lox_ir_data_initialize_array_node *) b;
            if (a_init_array->empty_initialization != b_init_array->empty_initialization ||
                a_init_array->n_elements != b_init_array->n_elements) {
                return false;
            }

            for(int i = 0; i < a_init_array->n_elements && !a_init_array->empty_initialization; i++){
                if(is_eq_lox_ir_data_node(a_init_array->elememnts[i], b_init_array->elememnts[i], allocator)){
                    return false;
                }
            }

            return true;
        }
        case LOX_IR_DATA_NODE_BINARY: {
            struct lox_ir_data_binary_node * a_binary = (struct lox_ir_data_binary_node *) a;
            struct lox_ir_data_binary_node * b_binary = (struct lox_ir_data_binary_node *) b;
            if (a_binary->operator != b_binary->operator) {
                return false;
            }
            bool is_commutative = is_commutative_associative_bytecode_instruction(a_binary->operator);
            if (!is_commutative) {
                return is_eq_lox_ir_data_node(a_binary->left, b_binary->left, allocator) &&
                       is_eq_lox_ir_data_node(a_binary->right, b_binary->right, allocator);
            }
            //Check for commuativiy equivalence
            if((is_eq_lox_ir_data_node(a_binary->left, b_binary->left, allocator) &&
                is_eq_lox_ir_data_node(a_binary->right, b_binary->right, allocator)) ||
               (is_eq_lox_ir_data_node(a_binary->left, b_binary->right, allocator) &&
                is_eq_lox_ir_data_node(a_binary->left, b_binary->right, allocator))) {
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
        case LOX_IR_DATA_NODE_CAST: {
            struct lox_ir_data_cast_node * a_cast = (struct lox_ir_data_cast_node *) a;
            struct lox_ir_data_cast_node * b_cast = (struct lox_ir_data_cast_node *) b;
            return is_eq_lox_ir_data_node(a_cast->to_cast, b_cast->to_cast, allocator);
        }
        default: return false;
    }
}

bool for_each_lox_ir_data_node(
        struct lox_ir_data_node * node,
        void ** parent_ptr,
        void * extra,
        long options,
        lox_ir_data_node_consumer_t consumer
) {
    return for_each_lox_ir_data_node_recursive(NULL, parent_ptr, node, extra, options, consumer);
}

static bool for_each_lox_ir_data_node_recursive(
        struct lox_ir_data_node * parent_current,
        void ** parent_current_ptr,
        struct lox_ir_data_node * current_node,
        void * extra,
        long options,
        lox_ir_data_node_consumer_t consumer
) {
    if(IS_FLAG_SET(options, LOX_IR_DATA_NODE_OPT_NOT_TERMINATORS) && is_terminator_lox_ir_data_node(current_node)){
        return true;
    }
    if(IS_FLAG_SET(options, LOX_IR_DATA_NODE_OPT_ONLY_TERMINATORS) && is_terminator_lox_ir_data_node(current_node)){
        consumer(parent_current, parent_current_ptr, current_node, extra);
        return true;
    }
    if(IS_FLAG_SET(options, LOX_IR_DATA_NODE_OPT_PRE_ORDER)){
        //Keep scanning but not from this control anymmore
        if(!consumer(parent_current, parent_current_ptr, current_node, extra)) {
            return true;
        }
    }

    struct u64_set children_ptr_set = get_children_lox_ir_data_node(current_node, NATIVE_LOX_ALLOCATOR());
    FOR_EACH_U64_SET_VALUE(children_ptr_set, struct lox_ir_data_node **, children_ptr) {
        for_each_lox_ir_data_node_recursive(current_node, (void **) children_ptr, *children_ptr, extra, options,
                                            consumer);
    }

    free_u64_set(&children_ptr_set);

    if (IS_FLAG_SET(options, LOX_IR_DATA_NODE_OPT_POST_ORDER)) {
        return consumer(parent_current, parent_current_ptr, current_node, extra);
    }

    return true;
}

struct lox_ir_data_constant_node * create_lox_ir_const_node(
        uint64_t constant_value,
        lox_ir_type_t type,
        struct bytecode_list * bytecode,
        struct lox_allocator * allocator
) {
    struct lox_ir_data_constant_node * constant_node = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_CONSTANT, struct lox_ir_data_constant_node, bytecode, allocator
    );

    constant_node->data.produced_type = CREATE_LOX_IR_TYPE(type, allocator);
    constant_node->value = constant_value;

    return constant_node;
}

static bool free_lox_data_node_consumer(
        struct lox_ir_data_node * _,
        void ** __,
        struct lox_ir_data_node * current_node,
        void * ___
) {
    NATIVE_LOX_FREE(current_node);
    return true;
}

void free_lox_ir_data_node(struct lox_ir_data_node * node_to_free) {
    for_each_lox_ir_data_node(
            node_to_free,
            NULL,
            NULL,
            LOX_IR_DATA_NODE_OPT_POST_ORDER,
            free_lox_data_node_consumer
    );
}

uint64_t mix_hash_not_commutative(uint64_t a, uint64_t b) {
    return a ^ (b * 0x9e3779b97f4a7c15);
}

uint64_t mix_hash_commutative(uint64_t a, uint64_t b) {
    return a ^ b;
}

uint64_t hash_lox_ir_data_node(struct lox_ir_data_node * node) {
    switch (node->type) {
        case LOX_IR_DATA_NODE_GET_V_REGISTER: {
            struct lox_ir_data_get_v_register_node * get_v_reg = (struct lox_ir_data_get_v_register_node *) node;
            return mix_hash_not_commutative(get_v_reg->v_register.number, (uint64_t) get_v_reg->v_register.is_float_register);
        }
        case LOX_IR_DATA_NODE_GUARD: {
            struct lox_ir_data_guard_node * guard = (struct lox_ir_data_guard_node *) node;
            return hash_lox_ir_data_node(guard->guard.value);
        }
        case LOX_IR_DATA_NODE_CONSTANT: {
            return GET_CONST_VALUE_LOX_IR_NODE(node);
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            struct lox_ir_data_get_array_length * get_array_length = (struct lox_ir_data_get_array_length *) node;
            return mix_hash_commutative(LOX_IR_DATA_NODE_GET_ARRAY_LENGTH,
                                        hash_lox_ir_data_node(get_array_length->instance));
        }
        case LOX_IR_DATA_NODE_GET_LOCAL: {
            struct lox_ir_data_get_local_node * get_local = (struct lox_ir_data_get_local_node *) node;
            return mix_hash_commutative(LOX_IR_DATA_NODE_GET_LOCAL, get_local->local_number);
        }
        case LOX_IR_DATA_NODE_GET_GLOBAL: {
            struct lox_ir_data_get_global_node * get_global = (struct lox_ir_data_get_global_node *) node;
            uint64_t package_name_hash = hash_string(get_global->package->name, strlen(get_global->package->name));
            uint64_t global_name_hash = get_global->name->hash;
            return mix_hash_not_commutative(package_name_hash, global_name_hash);
        }
        case LOX_IR_DATA_NODE_PHI: {
            uint64_t hash = 0;
            FOR_EACH_SSA_NAME_IN_PHI_NODE((struct lox_ir_data_phi_node *) node, current_ssa_name) {
                hash = mix_hash_commutative(hash, current_ssa_name.u16);
            }
            return hash;
        }
        case LOX_IR_DATA_NODE_GET_SSA_NAME: {
            struct lox_ir_data_get_ssa_name_node * get_ssa_name = (struct lox_ir_data_get_ssa_name_node *) node;
            return mix_hash_not_commutative(0, get_ssa_name->ssa_name.u16);
        }
        case LOX_IR_DATA_NODE_CALL: {
            struct lox_ir_data_function_call_node * call_node = (struct lox_ir_data_function_call_node *) node;
            uint64_t hash = call_node->is_native ? (uint64_t) call_node->lox_function.function : (uint64_t) call_node->native_function;
            for(int i = 0; i < call_node->n_arguments; i++){
                hash = mix_hash_not_commutative(hash, hash_lox_ir_data_node(call_node->arguments[i]));
            }
            return hash;
        }
        case LOX_IR_DATA_NODE_BINARY: {
            struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) node;
            uint64_t right_hash = hash_lox_ir_data_node(binary->right);
            uint64_t left_hash = hash_lox_ir_data_node(binary->left);

            uint64_t operand_hash = is_commutative_associative_bytecode_instruction(binary->operator) ?
                    mix_hash_commutative(left_hash, right_hash) :
                    mix_hash_not_commutative(left_hash, right_hash);
            return mix_hash_not_commutative(operand_hash, binary->operator);
        }
        case LOX_IR_DATA_NODE_UNARY: {
            struct lox_ir_data_unary_node * unary = (struct lox_ir_data_unary_node *) node;
            uint64_t unary_operand_hash = hash_lox_ir_data_node(unary->operand);
            return mix_hash_not_commutative(unary_operand_hash, unary->operator);
        }
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD: {
            struct lox_ir_data_get_struct_field_node * get_struct_field = (struct lox_ir_data_get_struct_field_node *) node;
            uint64_t instance_hash = hash_lox_ir_data_node(get_struct_field->instance);
            uint64_t field_name_hash = get_struct_field->field_name->hash;
            return mix_hash_not_commutative(instance_hash, field_name_hash);
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            struct lox_ir_data_initialize_struct_node * init_struct = (struct lox_ir_data_initialize_struct_node *) node;
            uint64_t hash = init_struct->definition->name->hash;
            for(int i = 0; i < init_struct->definition->n_fields; i++){
                hash = mix_hash_not_commutative(hash, hash_lox_ir_data_node(init_struct->fields_nodes[i]));
            }
            return hash;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT: {
            struct lox_ir_data_get_array_element_node * get_arr_ele = (struct lox_ir_data_get_array_element_node *) node;
            uint64_t array_instance_hash = hash_lox_ir_data_node(get_arr_ele->instance);
            uint64_t index_instance_hash = hash_lox_ir_data_node(get_arr_ele->index);
            return mix_hash_not_commutative(array_instance_hash, index_instance_hash);
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            struct lox_ir_data_initialize_array_node * init_array = (struct lox_ir_data_initialize_array_node *) node;
            uint64_t hash = init_array->n_elements;
            for (int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++) {
                hash = mix_hash_not_commutative(hash, hash_lox_ir_data_node(init_array->elememnts[i]));
            }
            return hash;
        }
        case LOX_IR_DATA_NODE_CAST: {
            struct lox_ir_data_cast_node * cast = (struct lox_ir_data_cast_node *) node;
            uint64_t hash = cast->to_cast->produced_type->type;
            return mix_hash_not_commutative(hash, hash_lox_ir_data_node(cast->to_cast));
        }
        default:
            runtime_panic("Illegal code path in hash_expression(struct lox_ir_data_node *)");
    }
}

struct uses_same_binary_operation_consumer_struct {
    bool uses_same_op;
    bytecode_t expected_operation;
};

static bool uses_same_binary_operation_consumer(
        struct lox_ir_data_node * _,
        void ** __,
        struct lox_ir_data_node * child,
        void * extra
) {
    struct uses_same_binary_operation_consumer_struct * consumer_struct = extra;
    if (child->type == LOX_IR_DATA_NODE_BINARY && consumer_struct->uses_same_op) {
        struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) child;
        if(binary->operator != consumer_struct->expected_operation){
            consumer_struct->uses_same_op = false;
        }
    }

    return true;
}

static bool uses_same_binary_operation(struct lox_ir_data_node * start_node, bytecode_t operation) {
    struct uses_same_binary_operation_consumer_struct consumer_struct = (struct uses_same_binary_operation_consumer_struct) {
        .expected_operation = operation,
        .uses_same_op = true,
    };

    for_each_lox_ir_data_node(
            start_node,
            NULL,
            &consumer_struct,
            LOX_IR_DATA_NODE_OPT_POST_ORDER,
            uses_same_binary_operation_consumer
    );

    return consumer_struct.uses_same_op;
}

static bool flat_out_binary_operand_nodes_consumer(
        struct lox_ir_data_node * _,
        void ** __,
        struct lox_ir_data_node * child,
        void * extra
) {
    struct u64_set * operands = extra;
    if (child->type != LOX_IR_DATA_NODE_BINARY) {
        add_u64_set(operands, (uint64_t) child);
    }
    return true;
}

static struct u64_set flat_out_binary_operand_nodes(struct lox_ir_data_node * node, struct lox_allocator * allocator) {
    struct u64_set operands;
    init_u64_set(&operands, allocator);

    for_each_lox_ir_data_node(
            node,
            NULL,
            &operands,
            LOX_IR_DATA_NODE_OPT_POST_ORDER,
            &flat_out_binary_operand_nodes_consumer
    );

    return operands;
}

static bool check_equivalence_flatted_out(struct u64_set left, struct u64_set right, struct lox_allocator * allocator) {
    FOR_EACH_U64_SET_VALUE(left, struct lox_ir_data_node *, current_left_data_node) {
        bool some_match_in_right_found = false;

        FOR_EACH_U64_SET_VALUE(right, struct lox_ir_data_node *, current_right_data_node) {
            if(is_eq_lox_ir_data_node(current_left_data_node, current_right_data_node, allocator)){
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

struct lox_ir_data_guard_node * create_guard_lox_ir_data_node(
        struct lox_ir_guard guard,
        struct lox_allocator * allocator
) {
    struct lox_ir_data_guard_node * guard_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GUARD, struct lox_ir_data_guard_node, NULL,
                                                                   allocator);
    guard_node->guard = guard;

    struct lox_ir_type * produced_type = NULL;

    switch (guard.type) {
        case LOX_IR_GUARD_TYPE_CHECK: {
            produced_type = CREATE_LOX_IR_TYPE(guard.value_to_compare.type, allocator);
            break;
        }
        case LOX_IR_GUARD_BOOLEAN_CHECK: {
            produced_type = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_NATIVE_BOOLEAN, allocator);
            break;
        }
        case LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK: {
            produced_type = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_STRUCT_INSTANCE, allocator);
            if(guard.value_to_compare.struct_definition != NULL){
                produced_type->value.struct_instance = LOX_MALLOC(allocator, sizeof(struct struct_instance_lox_ir_type));
                produced_type->value.struct_instance->definition = guard.value_to_compare.struct_definition;
                init_u64_hash_table(&produced_type->value.struct_instance->type_by_field_name, allocator);
            }
            break;
        }
        case LOX_IR_GUARD_ARRAY_TYPE_CHECK: {
            produced_type = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ARRAY, allocator);
            produced_type->value.array = LOX_MALLOC(allocator, sizeof(struct array_lox_ir_type));
            produced_type->value.array->type = CREATE_LOX_IR_TYPE(guard.value_to_compare.type, allocator);
            break;
        }
    }

    guard_node->data.produced_type = produced_type;

    return guard_node;
}

struct lox_ir_data_guard_node * create_from_profile_lox_ir_data_guard_node(
        struct type_profile_data type_profile,
        struct lox_ir_data_node * source,
        struct lox_allocator * allocator,
        lox_ir_guard_action_on_check_failed action_on_guard_failed
) {
    profile_data_type_t profiled_type = get_type_by_type_profile_data(type_profile);

    struct lox_ir_data_guard_node * guard_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GUARD, struct lox_ir_data_guard_node, NULL,
            allocator);
    guard_node->guard.action_on_guard_failed = action_on_guard_failed;
    guard_node->guard.value = source;
    guard_node->data.produced_type = source->produced_type;

    if(profiled_type == PROFILE_DATA_TYPE_STRUCT_INSTANCE && type_profile.invalid_struct_definition){
        guard_node->guard.type = LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK;
        guard_node->guard.value_to_compare.struct_definition = type_profile.struct_definition;
    } else {
        guard_node->guard.type = LOX_IR_GUARD_TYPE_CHECK;
        guard_node->guard.value_to_compare.type = profiled_type_to_lox_ir_type(profiled_type);
    }

    return guard_node;
}

struct u64_set get_children_lox_ir_data_node(struct lox_ir_data_node * node, struct lox_allocator * allocator) {
    struct u64_set children;
    init_u64_set(&children, allocator);

    switch (node->type) {
        case LOX_IR_DATA_NODE_CALL: {
            struct lox_ir_data_function_call_node * call_node = (struct lox_ir_data_function_call_node *) node;
            for(int i = 0 ; i < call_node->n_arguments; i++){
                add_u64_set(&children, (uint64_t) &call_node->arguments[i]);
            }
            break;
        }
        case LOX_IR_DATA_NODE_BINARY: {
            struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) node;
            add_u64_set(&children, (uint64_t) &binary->left);
            add_u64_set(&children, (uint64_t) &binary->right);
            break;
        }
        case LOX_IR_DATA_NODE_UNARY: {
            struct lox_ir_data_unary_node * unary = (struct lox_ir_data_unary_node *) node;
            add_u64_set(&children, (uint64_t) &unary->operand);
            break;
        }
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD: {
            struct lox_ir_data_get_struct_field_node * get_struct_field = (struct lox_ir_data_get_struct_field_node *) node;
            add_u64_set(&children, (uint64_t) &get_struct_field->instance);
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            struct lox_ir_data_initialize_struct_node * init_struct = (struct lox_ir_data_initialize_struct_node *) node;
            for(int i = 0; i < init_struct->definition->n_fields; i++){
                add_u64_set(&children, (uint64_t) &init_struct->fields_nodes[i]);
            }
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT: {
            struct lox_ir_data_get_array_element_node * get_arr_element = (struct lox_ir_data_get_array_element_node *) node;
            add_u64_set(&children, (uint64_t) &get_arr_element->instance);
            add_u64_set(&children, (uint64_t) &get_arr_element->index);
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            struct lox_ir_data_initialize_array_node * init_array = (struct lox_ir_data_initialize_array_node *) node;
            for(int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++){
                add_u64_set(&children, (uint64_t) &init_array->elememnts[i]);
            }
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            struct lox_ir_data_get_array_length * get_arr_len = (struct lox_ir_data_get_array_length *) node;
            add_u64_set(&children, (uint64_t) &get_arr_len->instance);
            break;
        }
        case LOX_IR_DATA_NODE_GUARD: {
            struct lox_ir_data_guard_node * guard = (struct lox_ir_data_guard_node *) node;
            add_u64_set(&children, (uint64_t) &guard->guard.value);
            break;
        }
        case LOX_IR_DATA_NODE_CAST: {
            struct lox_ir_data_cast_node * cast = (struct lox_ir_data_cast_node *) node;
            add_u64_set(&children, (uint64_t) &cast->to_cast);
            break;
        }
        case LOX_IR_DATA_NODE_GET_LOCAL:
        case LOX_IR_DATA_NODE_CONSTANT:
        case LOX_IR_DATA_NODE_PHI:
        case LOX_IR_DATA_NODE_GET_SSA_NAME:
        case LOX_IR_DATA_NODE_GET_V_REGISTER:
        case LOX_IR_DATA_NODE_GET_GLOBAL: {
            break;
        }
    }

    return children;
}

void const_to_lox_lox_ir_data_node(struct lox_ir_data_constant_node * const_node, lox_ir_type_t new_lox_type) {
    if (is_native_lox_ir_type(const_node->data.produced_type->type)) {
        const_node->value = value_native_to_lox_ir_type((lox_value_t) const_node->value, new_lox_type);
        const_node->data.produced_type->type = new_lox_type;
    }
}

void const_to_native_lox_ir_data_node(struct lox_ir_data_constant_node * const_node) {
    if (is_lox_lox_ir_type(const_node->data.produced_type->type)) {
        const_node->data.produced_type->type = lox_type_to_native_lox_ir_type(const_node->data.produced_type->type);
        const_node->value = value_lox_to_native_lox_ir_type((lox_value_t) const_node->value);
    }
}

bool is_marked_as_escaped_lox_ir_data_node(struct lox_ir_data_node * data_node) {
    switch (data_node->type) {
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD: {
            struct lox_ir_data_get_struct_field_node * get_struct_field = (struct lox_ir_data_get_struct_field_node *) data_node;
            return get_struct_field->escapes;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            struct lox_ir_data_initialize_struct_node * init_struct = (struct lox_ir_data_initialize_struct_node *) data_node;
            return init_struct->escapes;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT: {
            struct lox_ir_data_get_array_element_node * get_arr_ele = (struct lox_ir_data_get_array_element_node *) data_node;
            return get_arr_ele->escapes;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            struct lox_ir_data_initialize_array_node * init_array = (struct lox_ir_data_initialize_array_node *) data_node;
            return init_array->escapes;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            struct lox_ir_data_get_array_length * get_arr_length = (struct lox_ir_data_get_array_length *) data_node;
            return get_arr_length->escapes;
        }
        default:
            return false;
    }
}

void mark_as_escaped_lox_ir_data_node(struct lox_ir_data_node * data_node) {
    switch (data_node->type) {
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            struct lox_ir_data_get_array_length * get_array_length = (struct lox_ir_data_get_array_length *) data_node;
            get_array_length->escapes = true;
            break;
        }
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD: {
            struct lox_ir_data_get_struct_field_node * get_struct_field = (struct lox_ir_data_get_struct_field_node *) data_node;
            get_struct_field->escapes = true;
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            struct lox_ir_data_initialize_struct_node * init_struct = (struct lox_ir_data_initialize_struct_node *) data_node;
            init_struct->escapes = true;
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT: {
            struct lox_ir_data_get_array_element_node * get_arr_ele = (struct lox_ir_data_get_array_element_node *) data_node;
            get_arr_ele->escapes = true;
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            struct lox_ir_data_initialize_array_node * init_array = (struct lox_ir_data_initialize_array_node *) data_node;
            init_array->escapes = true;
            break;
        }
    }
}