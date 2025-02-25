#include "cast_insertion.h"

struct ci {
    struct lox_ir * lox_ir;
    struct u64_set processed_blocks;
    struct stack_list * blocks_to_reprocess;
    struct arena_lox_allocator ci_allocator;
};

static struct ci * alloc_cast_insertion(struct lox_ir *lox_ir);
static void free_casting_insertion(struct ci *ci);
static bool control_requires_lox_input(struct ci *ci, struct lox_ir_control_node *control);

static bool perform_cast_insertion_block(struct lox_ir_block *current_block, void *extra);
static void perform_cast_insertion_control(struct ci *ci, struct lox_ir_block *current_block, struct lox_ir_control_node *current_control);
static bool perform_cast_insertion_data_consumer(struct lox_ir_data_node *parent, void **parent_child_ptr, struct lox_ir_data_node *child, void *extra);
static void perform_cast_insertion_data(struct ci *ci, struct lox_ir_block *block, struct lox_ir_control_node *control, struct lox_ir_data_node *child_node,
                                        struct lox_ir_data_node *parent_node, void **parent_child_field_ptr, void ** parent_parent_field_ptr);
static bool is_ssa_name_lox(struct ci *ci, struct lox_ir_block *block, struct ssa_name ssa_name);
static void extract_define_cast_from_phi(struct ci*, struct lox_ir_block*, struct lox_ir_control_node*, struct lox_ir_data_phi_node *,
                                         struct ssa_name, lox_ir_type_t);
static lox_ir_type_t calculate_type_should_produce_data(struct ci*, struct lox_ir_block*, struct lox_ir_data_node*, bool);
static lox_ir_type_t calculate_data_input_type_should_produce(struct ci*, struct lox_ir_block*, struct lox_ir_data_node*, struct lox_ir_data_node*, bool);
static struct lox_ir_data_node * insert_lox_type_check_guard(struct ci*, struct lox_ir_data_node*, void**, lox_ir_type_t);
static void insert_cast_node(struct ci*, struct lox_ir_data_node*, void**, lox_ir_type_t);
static void cast_node(struct ci*, struct lox_ir_data_node*, struct lox_ir_control_node*, void**, lox_ir_type_t);
static bool data_always_requires_lox_input(struct lox_ir_data_node*);
static lox_ir_type_t calculate_expected_type_binary_to_produce(struct lox_ir_data_binary_node*,struct lox_ir_data_node*);
static lox_ir_type_t calculate_expected_type_child_to_produce(struct ci*, struct lox_ir_control_node*, struct lox_ir_data_node*, struct lox_ir_data_node*);
static lox_ir_type_t calculate_actual_type_child_produces(struct ci*, struct lox_ir_control_node*, struct lox_ir_data_node*, struct lox_ir_data_node*);
static lox_ir_type_t calculate_expected_type_unary_to_produce(struct lox_ir_data_unary_node*,struct lox_ir_data_node*);
static bool can_process(struct ci*, struct lox_ir_block*);
static void handle_phi_node(struct ci*,struct lox_ir_control_node*,struct lox_ir_data_phi_node*,lox_ir_type_t);
static void propagate_new_ssa_name_type(struct ci *ci, struct lox_ir_control_define_ssa_name_node *define);
static void reprocess_pending_blocks(struct ci *ci);
static lox_ir_type_t get_type_by_ssa_name(struct ci*, struct lox_ir_control_node*, struct ssa_name);
static void remove_cast_node(struct ci*, struct lox_ir_data_node*, struct lox_ir_data_node*,void**);

void perform_cast_insertion(struct lox_ir * lox_ir) {
    struct ci * ci = alloc_cast_insertion(lox_ir);

    for_each_lox_ir_block(
            lox_ir->first_block,
            &ci->ci_allocator.lox_allocator,
            ci,
            LOX_IR_BLOCK_OPT_REPEATED,
            &perform_cast_insertion_block
    );

    free_casting_insertion(ci);
}

static bool perform_cast_insertion_block(struct lox_ir_block * current_block, void * extra) {
    struct ci * ci = extra;

    if(!can_process(ci, current_block)){
        return true;
    }

    add_u64_set(&ci->processed_blocks, (uint64_t) current_block);

    for (struct lox_ir_control_node * current_control = current_block->first;; current_control = current_control->next) {
        perform_cast_insertion_control(ci, current_block, current_control);

        if (current_control == current_block->last) {
            break;
        }
    }

    reprocess_pending_blocks(ci);

    return true;
}

static void reprocess_pending_blocks(struct ci * ci) {
    struct stack_list * pending_to_reprocess = ci->blocks_to_reprocess;
    //Reset queue
    ci->blocks_to_reprocess = LOX_MALLOC(&ci->ci_allocator.lox_allocator, sizeof(struct stack_list));
    init_stack_list(ci->blocks_to_reprocess, &ci->ci_allocator.lox_allocator);

    while (!is_empty_stack_list(pending_to_reprocess)) {
        struct lox_ir_block * block_to_reprocess = pop_stack_list(pending_to_reprocess);

        for (struct lox_ir_control_node * current_control = block_to_reprocess->first;; current_control = current_control->next) {
            perform_cast_insertion_control(ci, block_to_reprocess, current_control);

            if (current_control == block_to_reprocess->last) {
                break;
            }
        }
    }
}

struct perform_cast_insertion_data_struct {
    struct lox_ir_control_node * control_node;
    struct lox_ir_block * block;
    struct ci * ci;
};

static void perform_cast_insertion_control(
        struct ci * ci,
        struct lox_ir_block * current_block,
        struct lox_ir_control_node * current_control
) {
    struct perform_cast_insertion_data_struct consumer_struct = (struct perform_cast_insertion_data_struct) {
        .control_node = current_control,
        .block = current_block,
        .ci = ci,
    };

    for_each_data_node_in_lox_ir_control(
            current_control,
            &consumer_struct,
            LOX_IR_DATA_NODE_OPT_PRE_ORDER,
            &perform_cast_insertion_data_consumer
    );

    if (current_control->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) current_control;
        put_type_by_ssa_name_lox_ir(ci->lox_ir, current_block, define->ssa_name, define->value->produced_type);
        propagate_new_ssa_name_type(ci, define);
    }
}

static bool perform_cast_insertion_data_consumer(
        struct lox_ir_data_node * parent,
        void ** parent_child_ptr,
        struct lox_ir_data_node * child,
        void * extra
) {
    struct perform_cast_insertion_data_struct * consumer_struct = extra;

    perform_cast_insertion_data(consumer_struct->ci, consumer_struct->block, consumer_struct->control_node,
                                child, parent, parent_child_ptr, NULL);

    //Only iterate parent
    return false;
}

static void perform_cast_insertion_data(
        struct ci * ci,
        struct lox_ir_block * block,
        struct lox_ir_control_node * control,
        struct lox_ir_data_node * child_node,
        struct lox_ir_data_node * parent_node,
        void ** parent_child_field_ptr, //Pointer to field of parent_node to child_node
        void ** parent_parent_field_ptr //Pointer to field of parent of the parent_node to parent_node
) {
    bool first_iteration = parent_node == NULL;

    FOR_EACH_U64_SET_VALUE(get_children_lox_ir_data_node(child_node, &ci->ci_allocator.lox_allocator), children_field_ptr_u64) {
        void ** children_field_ptr = (void **) children_field_ptr_u64;
        struct lox_ir_data_node * child = *((struct lox_ir_data_node **) children_field_ptr);

        perform_cast_insertion_data(ci, block, control, child, child_node, (void **) children_field_ptr,
                                    parent_child_field_ptr);
    }

    lox_ir_type_t expected_type = calculate_expected_type_child_to_produce(ci, control, parent_node, child_node);
    lox_ir_type_t actual_type = calculate_actual_type_child_produces(ci, control, parent_node, child_node);

    //Special case: We deal with string concatenation by emitting at compiletime a functino call to generic function to
    //concatenate strings, so we won't cast its operands
    bool is_string_concatenation_case = parent_node != NULL
            && parent_node->type == LOX_IR_DATA_NODE_BINARY
            && ((struct lox_ir_data_binary_node *) parent_node)->operator == OP_ADD
            && IS_STRING_LOX_IR_TYPE(parent_node->produced_type->type);

    if (actual_type != expected_type && !is_string_concatenation_case) {
        cast_node(ci, child_node, control, parent_child_field_ptr, expected_type);
    }
    if (actual_type == expected_type && parent_node != NULL && parent_node->type == LOX_IR_DATA_NODE_CAST) {
        remove_cast_node(ci, parent_node, child_node, parent_parent_field_ptr);
    }
    if (child_node->type == LOX_IR_DATA_NODE_PHI) {
        handle_phi_node(ci, control, (struct lox_ir_data_phi_node *) child_node, expected_type);
    }
}

static lox_ir_type_t calculate_actual_type_child_produces(
        struct ci * ci,
        struct lox_ir_control_node * control_node,
        struct lox_ir_data_node * parent,
        struct lox_ir_data_node * input
) {
    if (parent == NULL) {
        return !control_requires_lox_input(ci, control_node) ?
            input->produced_type->type :
            LOX_IR_TYPE_LOX_ANY;
    }

    if (input->type == LOX_IR_DATA_NODE_GET_SSA_NAME) {
        lox_ir_type_t new_type = get_type_by_ssa_name(ci, control_node, GET_USED_SSA_NAME(input));
        lox_ir_type_t current_type = input->produced_type->type;

        if (new_type != current_type) {
            input->produced_type->type = new_type;
        }
    }

    return input->produced_type->type;
}

static lox_ir_type_t calculate_expected_type_child_to_produce(
        struct ci * ci,
        struct lox_ir_control_node * control,
        struct lox_ir_data_node * parent,
        struct lox_ir_data_node * input
) {
    if (parent == NULL) {
        return control_requires_lox_input(ci, control) ?
            native_type_to_lox_ir_type(input->produced_type->type) :
            lox_type_to_native_lox_ir_type(input->produced_type->type);
    }

    switch (parent->type) {
        case LOX_IR_DATA_NODE_CALL: return LOX_IR_TYPE_LOX_ANY;
        case LOX_IR_DATA_NODE_GUARD: return LOX_IR_TYPE_LOX_ANY;
        case LOX_IR_DATA_NODE_UNARY: {
            return calculate_expected_type_unary_to_produce((struct lox_ir_data_unary_node *) parent, input);
        }
        case LOX_IR_DATA_NODE_BINARY: {
            return calculate_expected_type_binary_to_produce((struct lox_ir_data_binary_node *) parent, input);
        }
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD: {
            return is_marked_as_escaped_lox_ir_data_node(parent) ?
                   LOX_IR_TYPE_LOX_STRUCT_INSTANCE :
                   LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT: {
            struct lox_ir_data_get_array_element_node * get_arr_ele = (struct lox_ir_data_get_array_element_node *) parent;
            if(get_arr_ele->instance == input){
                return get_arr_ele->escapes ? LOX_IR_TYPE_LOX_ARRAY : LOX_IR_TYPE_NATIVE_ARRAY;
            } else {
                //Otherwise input is the index
                return LOX_IR_TYPE_NATIVE_I64;
            }
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY:
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            return is_marked_as_escaped_lox_ir_data_node(parent) ?
                   LOX_IR_TYPE_LOX_ANY :
                   LOX_IR_TYPE_UNKNOWN;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            return is_marked_as_escaped_lox_ir_data_node(parent) ? LOX_IR_TYPE_LOX_ARRAY : LOX_IR_TYPE_NATIVE_ARRAY;
        }
        case LOX_IR_DATA_NODE_CAST: {
            return parent->produced_type->type;
        }
    }
}

static bool contains_binary_operands_types(struct u8_set types, lox_ir_type_t left, lox_ir_type_t right) {
    return contains_u8_set(&types, left + 1) && contains_u8_set(&types, right + 1);
}

static bool both_binary_operand_types_are(struct u8_set types, lox_ir_type_t type) {
    return contains_u8_set(&types, type + 1) && size_u8_set(types) == 1;
}

static lox_ir_type_t calculate_expected_type_binary_to_produce(
        struct lox_ir_data_binary_node * binary,
        struct lox_ir_data_node * binary_operand
) {
    if (IS_STRING_LOX_IR_TYPE(binary->data.produced_type->type)) {
        return binary_operand->produced_type->type;
    }

    bool binary_operand_produces_lox_i64 = binary_operand->produced_type->type == LOX_IR_TYPE_LOX_I64;
    bool binary_operand_produces_f64 = binary_operand->produced_type->type == LOX_IR_TYPE_F64;

    struct u8_set operand_types;
    init_u8_set(&operand_types);
    //We use +1 to avoid the problem of inserting in the set an enum with 0 int value
    add_u8_set(&operand_types, binary->right->produced_type->type + 1);
    add_u8_set(&operand_types, binary->left->produced_type->type + 1);

    if (both_binary_operand_types_are(operand_types, LOX_IR_TYPE_LOX_ANY)) {
        return LOX_IR_TYPE_F64;
    } else if (both_binary_operand_types_are(operand_types, LOX_IR_TYPE_LOX_I64)) {
        return LOX_IR_TYPE_NATIVE_I64;
    } else if (both_binary_operand_types_are(operand_types, LOX_IR_TYPE_F64)) {
        return LOX_IR_TYPE_F64;
    } else if (both_binary_operand_types_are(operand_types, LOX_IR_TYPE_NATIVE_I64)) {
        return LOX_IR_TYPE_NATIVE_I64;
    } else if (contains_binary_operands_types(operand_types, LOX_IR_TYPE_NATIVE_I64, LOX_IR_TYPE_F64)) {
        return LOX_IR_TYPE_F64;
    } else if (contains_binary_operands_types(operand_types, LOX_IR_TYPE_NATIVE_I64, LOX_IR_TYPE_LOX_ANY)) {
        return LOX_IR_TYPE_F64;
    } else if (contains_binary_operands_types(operand_types, LOX_IR_TYPE_NATIVE_I64, LOX_IR_TYPE_LOX_I64)) {
        return LOX_IR_TYPE_NATIVE_I64;
    } else if (contains_binary_operands_types(operand_types, LOX_IR_TYPE_F64, LOX_IR_TYPE_LOX_I64)) {
        return binary_operand_produces_f64 ? LOX_IR_TYPE_F64 : LOX_IR_TYPE_LOX_I64;
    } else if (contains_binary_operands_types(operand_types, LOX_IR_TYPE_LOX_I64, LOX_IR_TYPE_LOX_ANY)) {
        return binary_operand_produces_lox_i64 ? LOX_IR_TYPE_LOX_I64 : LOX_IR_TYPE_F64;
    } else if (contains_binary_operands_types(operand_types, LOX_IR_TYPE_F64, LOX_IR_TYPE_LOX_ANY)) {
        return LOX_IR_TYPE_F64;
    } else {
        //TODO Runtime error
    }
}

static lox_ir_type_t calculate_expected_type_unary_to_produce(
        struct lox_ir_data_unary_node * unary,
        struct lox_ir_data_node * operand
) {
    switch (unary->operator) {
        case UNARY_OPERATION_TYPE_NOT: return LOX_IR_TYPE_NATIVE_BOOLEAN;
        case UNARY_OPERATION_TYPE_NEGATION: {
            lox_ir_type_t operand_produced_type = operand->produced_type->type;
            return operand_produced_type || operand_produced_type ?
                   LOX_IR_TYPE_F64 :
                   LOX_IR_TYPE_NATIVE_I64;
        }
    }
}

static void cast_node(
        struct ci * ci,
        struct lox_ir_data_node * data_node,
        struct lox_ir_control_node * control_node,
        void ** parent_field_ptr,
        lox_ir_type_t type_should_produce
) {
    bool native_to_lox_cast = is_lox_lox_ir_type(type_should_produce) || type_should_produce == LOX_IR_TYPE_F64;
    bool lox_to_native_cast = !native_to_lox_cast;
    struct lox_ir_block * block = control_node->block;

    switch (data_node->type) {
        case LOX_IR_DATA_NODE_CONSTANT: {
            if (lox_to_native_cast) {
                const_to_native_lox_ir_data_node((struct lox_ir_data_constant_node *) data_node);
            }
            break;
        }
        case LOX_IR_DATA_NODE_CALL:
        case LOX_IR_DATA_NODE_GET_GLOBAL: {
            insert_cast_node(ci, data_node, parent_field_ptr, type_should_produce);
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT:
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD:
        case LOX_IR_DATA_NODE_GUARD: {
            insert_cast_node(ci, data_node, parent_field_ptr, type_should_produce);
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            data_node->produced_type->type = native_to_lox_cast ? LOX_IR_TYPE_LOX_STRUCT_INSTANCE : LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE;
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            data_node->produced_type->type = LOX_IR_TYPE_NATIVE_I64;
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            data_node->produced_type->type = native_to_lox_cast ? LOX_IR_TYPE_LOX_ARRAY : LOX_IR_TYPE_NATIVE_ARRAY;
            break;
        }
        case LOX_IR_DATA_NODE_BINARY:
        case LOX_IR_DATA_NODE_UNARY: {
            data_node->produced_type->type = type_should_produce;
            break;
        }
        case LOX_IR_DATA_NODE_GET_SSA_NAME: {
            data_node->produced_type = get_type_by_ssa_name_lox_ir(ci->lox_ir, block, GET_USED_SSA_NAME(data_node));
            if (data_node->produced_type->type != type_should_produce) {
                insert_cast_node(ci, data_node, parent_field_ptr, type_should_produce);
            }
            break;
        }
        case LOX_IR_DATA_NODE_PHI: {
            //Handled later by later call to handle_phi_node()
            break;
        }
    }
}

static void remove_cast_node(
        struct ci * ci,
        struct lox_ir_data_node * parent,
        struct lox_ir_data_node * child,
        void ** parent_field_ptr
) {
    *parent_field_ptr = child;
}

static void handle_phi_node(
        struct ci * ci,
        struct lox_ir_control_node * control_node,
        struct lox_ir_data_phi_node * phi,
        lox_ir_type_t expected_type_to_produce
) {
    struct lox_ir_block * block = control_node->block;
    bool produced_any = phi->data.produced_type->type == LOX_IR_TYPE_LOX_ANY;
    bool phi_node_should_produce_native = !produced_any;
    bool phi_node_should_produce_lox = produced_any;

    FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, phi_ssa_name) {
        bool is_ssa_name_native = is_ssa_name_lox(ci, block, phi_ssa_name);
        bool needs_to_be_extracted = (is_ssa_name_native && phi_node_should_produce_lox)
                                     || (!is_ssa_name_native && phi_node_should_produce_native);

        if (needs_to_be_extracted) {
            extract_define_cast_from_phi(ci, block, control_node, phi, phi_ssa_name, expected_type_to_produce);
        }
    }
}

static void insert_cast_node(
        struct ci * ci,
        struct lox_ir_data_node * to_cast,
        void ** parent,
        lox_ir_type_t type_should_produce
) {
    if(to_cast->produced_type->type == LOX_IR_TYPE_LOX_ANY){
        struct lox_ir_data_node * guard_node = insert_lox_type_check_guard(ci, to_cast, parent, type_should_produce);
        *parent = (void *) guard_node;
        return;
    }

    struct lox_ir_data_cast_node * cast_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_CAST, struct lox_ir_data_cast_node,
            NULL, LOX_IR_ALLOCATOR(ci->lox_ir));
    cast_node->to_cast = to_cast;
    cast_node->data.produced_type = create_lox_ir_type(type_should_produce, LOX_IR_ALLOCATOR(ci->lox_ir));

    *parent = (void *) cast_node;
}

static struct lox_ir_data_node * insert_lox_type_check_guard(
        struct ci * ci,
        struct lox_ir_data_node * node,
        void ** parnet_field_ptr,
        lox_ir_type_t type_to_be_checked
) {
    struct lox_ir_data_guard_node * guard_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GUARD, struct lox_ir_data_guard_node, NULL,
        LOX_IR_ALLOCATOR(ci->lox_ir));

    guard_node->guard.value = node;
    guard_node->guard.type = LOX_IR_GUARD_TYPE_CHECK;
    guard_node->guard.value_to_compare.type = type_to_be_checked;
    guard_node->guard.action_on_guard_failed = LOX_IR_GUARD_FAIL_ACTION_TYPE_RUNTIME_ERROR;
    guard_node->data.produced_type = create_lox_ir_type(type_to_be_checked, LOX_IR_ALLOCATOR(ci->lox_ir));

    if(parnet_field_ptr != NULL){
        *parnet_field_ptr = (void *) guard_node;
    }

    return &guard_node->data;
}

//Given a2 = phi(a0, a1) Extract: a1, it will produce: a3 = cast(a1); a2 = phi(a0, a3)
static void extract_define_cast_from_phi(
        struct ci * ci,
        struct lox_ir_block * block,
        struct lox_ir_control_node * control_uses_phi,
        struct lox_ir_data_phi_node * phi_node,
        struct ssa_name ssa_name_to_extract,
        lox_ir_type_t type_to_be_casted
) {
    struct ssa_name casted_ssa_name = alloc_ssa_version_lox_ir(ci->lox_ir, ssa_name_to_extract.value.local_number);

    struct lox_ir_control_define_ssa_name_node * define_casted = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME, struct lox_ir_control_define_ssa_name_node, block, LOX_IR_ALLOCATOR(ci->lox_ir)
    );
    struct lox_ir_data_get_ssa_name_node * get_uncasted_ssa_name_node = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, LOX_IR_ALLOCATOR(ci->lox_ir)
    );

    //get_uncasted_ssa_name_node
    get_uncasted_ssa_name_node->data.produced_type = get_type_by_ssa_name_lox_ir(ci->lox_ir, block, ssa_name_to_extract);
    get_uncasted_ssa_name_node->ssa_name = ssa_name_to_extract;
    add_ssa_name_use_lox_ir(ci->lox_ir, ssa_name_to_extract, &define_casted->control);

    //define casted node
    define_casted->ssa_name = casted_ssa_name;
    define_casted->value = &get_uncasted_ssa_name_node->data;
    add_u64_set(&block->defined_ssa_names, casted_ssa_name.u16);
    put_u64_hash_table(&ci->lox_ir->definitions_by_ssa_name, casted_ssa_name.u16, define_casted);
    add_before_control_node_lox_ir_block(block, control_uses_phi, &define_casted->control);

    //Insert cast node
    insert_cast_node(ci, define_casted->value, (void**) &define_casted->value, type_to_be_casted);

    //control_uses_phi
    remove_u64_set(&phi_node->ssa_versions, ssa_name_to_extract.value.version);
    add_u64_set(&phi_node->ssa_versions, casted_ssa_name.value.version);
    add_ssa_name_use_lox_ir(ci->lox_ir, casted_ssa_name, &define_casted->control);
    remove_ssa_name_use_lox_ir(ci->lox_ir, ssa_name_to_extract, control_uses_phi);
}

static bool is_ssa_name_lox(struct ci * ci, struct lox_ir_block * block, struct ssa_name ssa_name) {
    struct lox_ir_type * type = get_type_by_ssa_name_lox_ir(ci->lox_ir, block, ssa_name);
    return type->type == LOX_IR_TYPE_F64 || is_native_lox_ir_type(type->type);
}

static lox_ir_type_t get_type_by_ssa_name(struct ci * ci, struct lox_ir_control_node * control, struct ssa_name name) {
    struct lox_ir_type * type = get_type_by_ssa_name_lox_ir(ci->lox_ir, control->block, name);
    return type->type;
}

static bool control_requires_lox_input(struct ci * ci, struct lox_ir_control_node * control) {
    switch (control->type) {
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT:
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD:
            return is_marked_as_escaped_lox_ir_control(control);
        case LOX_IR_CONTORL_NODE_SET_GLOBAL:
        case LOX_IR_CONTROL_NODE_RETURN:
            return true;
        case LOX_IR_CONTROL_NODE_PRINT:
        case LOX_IR_CONTROL_NODE_DATA:
        case LOX_IR_CONTROL_NODE_ENTER_MONITOR:
        case LOX_IR_CONTROL_NODE_EXIT_MONITOR:
        case LOX_IR_CONTORL_NODE_SET_LOCAL:
        case LOX_IR_CONTROL_NODE_LOOP_JUMP:
        case LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP:
        case LOX_IR_CONTROL_NODE_GUARD:
            return false;
        case LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME: {
            struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) control;
            struct u64_set * uses_ssa_name = get_u64_hash_table(&ci->lox_ir->node_uses_by_ssa_name, define->ssa_name.u16);
            if(uses_ssa_name == NULL){
                return false;
            }

            bool some_use_requires_lox_type_as_input = false;

            FOR_EACH_U64_SET_VALUE(*uses_ssa_name, node_uses_ssa_name_ptr_u64) {
                struct lox_ir_control_node * node_uses_ssa_name = (struct lox_ir_control_node *) node_uses_ssa_name_ptr_u64;

                if(node_uses_ssa_name->type != LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT &&
                   node_uses_ssa_name->type != LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD &&
                   node_uses_ssa_name->type != LOX_IR_CONTORL_NODE_SET_GLOBAL &&
                   node_uses_ssa_name->type != LOX_IR_CONTROL_NODE_RETURN){
                    some_use_requires_lox_type_as_input = true;
                    break;
                }
            }

            return !some_use_requires_lox_type_as_input;
        }
    }
}

//If the current type is native, it means it has changed so we will "propagate" the type change by reprocessing
//the blocks where the ssa name is used and have been already processed. This is useful in loops where in the body
//a variable is used in the loop condition but assigned in the loop body
static void propagate_new_ssa_name_type(struct ci * ci, struct lox_ir_control_define_ssa_name_node * define) {
    if (is_native_lox_ir_type(define->value->produced_type->type)) {
        struct lox_ir_block * current_block = define->control.block;

        //1. We insert in the set the blocks that we will reprocess. We store in a set to a void duplicates
        struct u64_set blocks_to_reprocess;
        init_u64_set(&blocks_to_reprocess, &ci->ci_allocator.lox_allocator);
        struct u64_set * name_uses = get_u64_hash_table(&ci->lox_ir->node_uses_by_ssa_name, define->ssa_name.u16);
        FOR_EACH_U64_SET_VALUE(*name_uses, name_control_node_use_ptru_64) {
            struct lox_ir_control_node * control = (struct lox_ir_control_node *) name_control_node_use_ptru_64;
            struct lox_ir_block * block_to_maybe_reprocess = control->block;

            if (contains_u64_set(&ci->processed_blocks, (uint64_t) block_to_maybe_reprocess)
                && block_to_maybe_reprocess != current_block) {

                add_u64_set(&blocks_to_reprocess, (uint64_t) control->block);
            }
        }
        //2. We insert them in the stack
        FOR_EACH_U64_SET_VALUE(blocks_to_reprocess, block_to_reprocess_u64_ptr) {
            struct lox_ir_block * block_to_reprocess = (struct lox_ir_block *) block_to_reprocess_u64_ptr;
            push_stack_list(ci->blocks_to_reprocess, block_to_reprocess);
        }
    }
}

static bool can_process(struct ci * ci, struct lox_ir_block * block) {
    if (contains_u64_set(&ci->processed_blocks, (uint64_t) block)) {
        return false;
    }

    bool can_process = true;
    FOR_EACH_U64_SET_VALUE(block->predecesors, current_predecessor) {
        if(!contains_u64_set(&ci->processed_blocks, current_predecessor)){
            can_process = false;
            break;
        }
    }

    return can_process;
}

static struct ci * alloc_cast_insertion(struct lox_ir * lox_ir) {
    struct ci * ci =  NATIVE_LOX_MALLOC(sizeof(struct ci));
    ci->lox_ir = lox_ir;
    struct arena arena;
    init_arena(&arena);
    ci->ci_allocator = to_lox_allocator_arena(arena);
    init_u64_set(&ci->processed_blocks, &ci->ci_allocator.lox_allocator);
    ci->blocks_to_reprocess = LOX_MALLOC(&ci->ci_allocator.lox_allocator, sizeof(struct stack_list));
    init_stack_list(ci->blocks_to_reprocess, &ci->ci_allocator.lox_allocator);

    return ci;
}

static void free_casting_insertion(struct ci * ci) {
    free_arena(&ci->ci_allocator.arena);
    NATIVE_LOX_FREE(ci);
}