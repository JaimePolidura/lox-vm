#include "cast_insertion.h"

struct ci {
    struct lox_ir * lox_ir;
    struct u64_set processed_blocks;
    struct u64_set processed_ssa_names;

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
static lox_ir_type_t calculate_type_should_produce_data(struct ci*, struct lox_ir_block*, struct lox_ir_data_node*, bool);
static lox_ir_type_t calculate_data_input_type_should_produce(struct ci*, struct lox_ir_block*, struct lox_ir_data_node*, struct lox_ir_data_node*, bool);
static lox_ir_type_t calculate_actual_type_phi_produces(struct ci*,struct lox_ir_control_define_ssa_name_node*, struct lox_ir_data_phi_node*);
static struct lox_ir_data_guard_node * insert_lox_type_check_guard(struct ci*, struct lox_ir_data_node*, void**, lox_ir_type_t);
static void insert_cast_node(struct ci*, struct lox_ir_data_node*, void**, lox_ir_type_t);
static void cast_node(struct ci*, struct lox_ir_data_node*, struct lox_ir_control_node*, void**, lox_ir_type_t);
static bool data_always_requires_lox_input(struct lox_ir_data_node*);
static lox_ir_type_t calculate_expected_type_binary_to_produce(struct lox_ir_data_binary_node*,struct lox_ir_data_node*);
static lox_ir_type_t calculate_expected_type_child_to_produce(struct ci*, struct lox_ir_control_node*, struct lox_ir_data_node*, struct lox_ir_data_node*);
static lox_ir_type_t calculate_actual_type_child_produces(struct ci*, struct lox_ir_control_node*, struct lox_ir_data_node*, struct lox_ir_data_node*);
static lox_ir_type_t calculate_expected_type_unary_to_produce(struct lox_ir_data_unary_node*,struct lox_ir_data_node*);
static bool can_process(struct ci*, struct lox_ir_block*);
static void handle_phi_node(struct ci*,struct lox_ir_control_node*,struct lox_ir_data_phi_node*,lox_ir_type_t);
static lox_ir_type_t get_type_by_ssa_name(struct ci*, struct lox_ir_control_node*, struct ssa_name);
static void remove_cast_node(struct ci*, struct lox_ir_data_node*, struct lox_ir_data_node*,void**);
static void cast_binary_node(struct ci*, struct lox_ir_data_binary_node*, void ** parent_field_ptr,lox_ir_type_t);
static lox_ir_type_t calculate_type_produced_by_binary(struct lox_ir_data_binary_node *binary);
static void cast_unary_node(struct ci*, struct lox_ir_data_unary_node*, void ** parent_field_ptr,lox_ir_type_t);
static void update_prev_parent_produced_type(struct ci *ci, struct lox_ir_data_node *data_node);
static lox_ir_type_t union_binary_format_lox_type(lox_ir_type_t left, lox_ir_type_t right);
static struct u64_set calculate_predecessors_extract_cast_from_phi(struct ci*,struct ssa_name,struct lox_ir_block*);
static struct lox_ir_block * create_block_loop_case_extract_cast_from_phi(struct ci*,struct lox_ir_block*,struct lox_ir_block*);
static void insert_cast_node_and_replace_phi(struct ci*,struct ssa_name,struct lox_ir_block*,struct lox_ir_block*,
        struct lox_ir_control_node*,struct lox_ir_data_phi_node*,lox_ir_type_t);
static void extract_cast_from_phi(struct ci*,struct lox_ir_block*,struct lox_ir_control_node*,struct lox_ir_data_phi_node*,
        struct ssa_name,lox_ir_type_t);

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

    if (!can_process(ci, current_block)) {
        return true;
    }

    add_u64_set(&ci->processed_blocks, (uint64_t) current_block);

    for (struct lox_ir_control_node * current_control = current_block->first;; current_control = current_control->next) {
        perform_cast_insertion_control(ci, current_block, current_control);

        if (current_control == current_block->last) {
            break;
        }
    }

    return true;
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
        add_u64_set(&ci->processed_ssa_names, define->ssa_name.u16);
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

    FOR_EACH_U64_SET_VALUE(get_children_lox_ir_data_node(child_node, &ci->ci_allocator.lox_allocator), void **, children_field_ptr) {
        struct lox_ir_data_node * child = *((struct lox_ir_data_node **) children_field_ptr);

        perform_cast_insertion_data(ci, block, control, child, child_node, (void **) children_field_ptr,
                                    parent_child_field_ptr);
    }

    //At this point all inputs to child_node have been processed, so we can upate its produced type safely
    update_prev_parent_produced_type(ci, child_node);

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
    if (input->type == LOX_IR_DATA_NODE_PHI) {
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) control_node;
        struct lox_ir_data_phi_node * phi = (struct lox_ir_data_phi_node *) input;
        lox_ir_type_t produced_type = calculate_actual_type_phi_produces(ci, define, phi);
        input->produced_type->type = produced_type;
    }
    if (input->type == LOX_IR_DATA_NODE_GET_SSA_NAME) {
        lox_ir_type_t produced_type = get_type_by_ssa_name(ci, control_node, GET_USED_SSA_NAME(input));
        input->produced_type->type = produced_type;
    }
    if ((input->type == LOX_IR_DATA_NODE_GET_STRUCT_FIELD || input->type == LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT)
        && input->produced_type->type != LOX_IR_TYPE_LOX_ANY
        && !is_marked_as_escaped_lox_ir_data_node(input)) {
        input->produced_type->type = lox_type_to_native_lox_ir_type(input->produced_type->type);
    }

    return input->produced_type->type;
}

static lox_ir_type_t calculate_actual_type_phi_produces(
        struct ci * ci,
        struct lox_ir_control_define_ssa_name_node * define_node,
        struct lox_ir_data_phi_node * phi
) {
    if (phi->data.produced_type->type == LOX_IR_TYPE_LOX_ANY) {
        return LOX_IR_TYPE_LOX_ANY;
    }

    //Not assigned yet
    lox_ir_type_t type_produced = LOX_IR_TYPE_UNKNOWN;

    FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, ssa_name_used_in_phi) {
        if (is_cyclic_ssa_name_definition_lox_ir(ci->lox_ir, define_node->ssa_name, ssa_name_used_in_phi)) {
            continue;
        }

        lox_ir_type_t produced_type = get_type_by_ssa_name(ci, &define_node->control, ssa_name_used_in_phi);

        type_produced = type_produced != LOX_IR_TYPE_UNKNOWN ?
                union_binary_format_lox_type(type_produced, produced_type) :
                produced_type;
    }

    return type_produced;
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
            lox_ir_type_t expected_input_type = calculate_expected_type_unary_to_produce((struct lox_ir_data_unary_node *) parent, input);
            parent->produced_type->type = expected_input_type;
            return expected_input_type;
        }
        case LOX_IR_DATA_NODE_BINARY: {
            return calculate_expected_type_binary_to_produce((struct lox_ir_data_binary_node *) parent, input);
        }
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD: {
            return LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT: {
            struct lox_ir_data_get_array_element_node * get_arr_ele = (struct lox_ir_data_get_array_element_node *) parent;
            if(get_arr_ele->instance == input) {
                return LOX_IR_TYPE_NATIVE_ARRAY;
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
    if (binary->operator == OP_MODULO) {
        return LOX_IR_TYPE_NATIVE_I64;
    }
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
        case LOX_IR_DATA_NODE_BINARY: {
            cast_binary_node(ci, (struct lox_ir_data_binary_node *) data_node, parent_field_ptr, type_should_produce);
            break;
        }
        case LOX_IR_DATA_NODE_UNARY: {
            cast_unary_node(ci, (struct lox_ir_data_unary_node *) data_node, parent_field_ptr, type_should_produce);
            break;
        }
        case LOX_IR_DATA_NODE_GET_SSA_NAME: {
            data_node->produced_type = clone_lox_ir_type(get_type_by_ssa_name_lox_ir(ci->lox_ir, block, GET_USED_SSA_NAME(data_node)),
                                                         LOX_IR_ALLOCATOR(ci->lox_ir));
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

static void cast_unary_node(
        struct ci * ci,
        struct lox_ir_data_unary_node * unary,
        void ** parent_field_ptr,
        lox_ir_type_t expected_type
) {
    lox_ir_type_t actual_type = unary->operand->produced_type->type;

    unary->data.produced_type->type = actual_type;
    if (actual_type != expected_type) {
        insert_cast_node(ci, &unary->data, parent_field_ptr, expected_type);
    }
}

static void cast_binary_node(
        struct ci * ci,
        struct lox_ir_data_binary_node * binary,
        void ** parent_field_ptr,
        lox_ir_type_t expected_type
) {
    lox_ir_type_t actual_type = calculate_type_produced_by_binary(binary);

    binary->data.produced_type->type = actual_type;
    if (actual_type != expected_type) {
        insert_cast_node(ci, &binary->data, parent_field_ptr, expected_type);
    }
}

static lox_ir_type_t calculate_type_produced_by_binary(struct lox_ir_data_binary_node * binary) {
    if (binary->operator == OP_MODULO) {
        return LOX_IR_TYPE_NATIVE_I64;
    }
    if (is_comparation_bytecode_instruction(binary->operator)) {
        return LOX_IR_TYPE_NATIVE_BOOLEAN;
    }
    if (IS_STRING_LOX_IR_TYPE(binary->data.produced_type->type)) {
        return LOX_IR_TYPE_NATIVE_STRING;
    }

    lox_ir_type_t right_type = binary->right->produced_type->type;
    lox_ir_type_t left_type = binary->left->produced_type->type;

    if (left_type == LOX_IR_TYPE_F64 || right_type == LOX_IR_TYPE_F64) {
        return LOX_IR_TYPE_F64;
    } else if (left_type == LOX_IR_TYPE_LOX_I64 || right_type == LOX_IR_TYPE_LOX_I64) {
        return LOX_IR_TYPE_LOX_I64;
    } else {
        return LOX_IR_TYPE_NATIVE_I64;
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
    struct lox_ir_control_define_ssa_name_node * definition = (struct lox_ir_control_define_ssa_name_node *) control_node;
    struct lox_ir_block * block = control_node->block;
    bool produced_any = phi->data.produced_type->type == LOX_IR_TYPE_LOX_ANY;
    bool phi_node_should_produce_native = !produced_any;
    bool phi_node_should_produce_lox = produced_any;

    FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, phi_ssa_name) {
        bool is_ssa_name_native = is_ssa_name_lox(ci, block, phi_ssa_name);
        bool needs_to_be_extracted = (is_ssa_name_native && phi_node_should_produce_lox)
                                     || (!is_ssa_name_native && phi_node_should_produce_native);

        if (!is_cyclic_ssa_name_definition_lox_ir(ci->lox_ir, definition->ssa_name, phi_ssa_name) && needs_to_be_extracted) {
            extract_cast_from_phi(ci, block, control_node, phi, phi_ssa_name, expected_type_to_produce);
        }
    }

    if (phi_node_should_produce_native) {
        phi->data.produced_type->type = lox_type_to_native_lox_ir_type(phi->data.produced_type->type);
    }
}

static void insert_cast_node(
        struct ci * ci,
        struct lox_ir_data_node * to_cast,
        void ** parent,
        lox_ir_type_t expeced_type
) {
    if (to_cast->produced_type->type == LOX_IR_TYPE_LOX_ANY) {
        lox_ir_type_t type_to_be_used_in_guard = native_type_to_lox_ir_type(expeced_type);
        struct lox_ir_data_guard_node * guard_node = insert_lox_type_check_guard(ci, to_cast, parent, type_to_be_used_in_guard);
        *parent = (void *) guard_node;
        to_cast = &guard_node->data;
    }

    lox_ir_type_t actual_type = to_cast->produced_type->type;

    bool redundant_cast = actual_type == expeced_type ||
            is_same_number_binay_format_lox_ir_type(actual_type, expeced_type);

    if (!redundant_cast) {
        struct lox_ir_data_cast_node * cast_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_CAST, struct lox_ir_data_cast_node,
                NULL, LOX_IR_ALLOCATOR(ci->lox_ir));
        cast_node->to_cast = to_cast;
        cast_node->data.produced_type = create_lox_ir_type(expeced_type, LOX_IR_ALLOCATOR(ci->lox_ir));

        *parent = (void *) cast_node;
    }
}

static struct lox_ir_data_guard_node * insert_lox_type_check_guard(
        struct ci * ci,
        struct lox_ir_data_node * node,
        void ** parnet_field_ptr,
        lox_ir_type_t type_to_be_checked
) {
    struct lox_ir_guard guard;
    guard.value = node;
    guard.type = LOX_IR_GUARD_TYPE_CHECK;
    guard.value_to_compare.type = type_to_be_checked;
    guard.action_on_guard_failed = LOX_IR_GUARD_FAIL_ACTION_TYPE_RUNTIME_ERROR;

    struct lox_ir_data_guard_node * guard_node = create_guard_lox_ir_data_node(guard, LOX_IR_ALLOCATOR(ci->lox_ir));

    if(parnet_field_ptr != NULL){
        *parnet_field_ptr = (void *) guard_node;
    }

    return guard_node;
}

//Given a2 = phi(a0, a1) Extract: a1, it will produce: a3 = cast(a1); a2 = phi(a0, a3)
//We will move a3 = cast(a1) to a new block which is the closest to the definition, if it is not possible,
//we will create a new one.
static void extract_cast_from_phi(
        struct ci * ci,
        struct lox_ir_block * block_uses_casted,
        struct lox_ir_control_node * control_uses_phi,
        struct lox_ir_data_phi_node * phi_node,
        struct ssa_name ssa_name_to_extract,
        lox_ir_type_t type_to_be_casted
) {
    struct lox_ir_block * block_definition_uncasted = ((struct lox_ir_control_node *) get_u64_hash_table(
            &ci->lox_ir->definitions_by_ssa_name, ssa_name_to_extract.u16))->block;

    struct u64_set predecessorss_cast_will_be_held = calculate_predecessors_extract_cast_from_phi(ci,
            ssa_name_to_extract, block_uses_casted);

    struct lox_ir_block * block_definition_casted = NULL;
    if (block_definition_uncasted->nested_loop_body > block_uses_casted->nested_loop_body) {
        struct lox_ir_block * block_condition_casted = create_block_loop_case_extract_cast_from_phi(ci,
                block_definition_uncasted, block_uses_casted);

        insert_block_before_lox_ir(ci->lox_ir, block_condition_casted, predecessorss_cast_will_be_held, block_uses_casted);
        block_definition_casted = block_condition_casted->next_as.branch.true_branch;

    } else {
        block_definition_casted = alloc_lox_ir_block(LOX_IR_ALLOCATOR(ci->lox_ir));
        insert_block_before_lox_ir(ci->lox_ir, block_definition_casted, predecessorss_cast_will_be_held, block_uses_casted);
    }

    insert_cast_node_and_replace_phi(ci, ssa_name_to_extract, block_definition_casted, block_uses_casted,
            control_uses_phi, phi_node, type_to_be_casted);
}

static struct lox_ir_block * create_block_loop_case_extract_cast_from_phi(
        struct ci * ci,
        struct lox_ir_block * blocK_definition_uncasted,
        struct lox_ir_block * block_uses_casted
) {
    if (ci->lox_ir->first_block->is_loop_condition) {
        struct lox_ir_block * new_start_block = alloc_lox_ir_block(LOX_IR_ALLOCATOR(ci->lox_ir));
        new_start_block->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
        new_start_block->next_as.next = ci->lox_ir->first_block;
        add_u64_set(&ci->lox_ir->first_block->predecesors, (uint64_t) new_start_block);
        ci->lox_ir->first_block = new_start_block;
    }

    //Initialize entered loop flag to false
    struct lox_ir_control_define_ssa_name_node * define_entered_loop_flag = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME,
            struct lox_ir_control_define_ssa_name_node, ci->lox_ir->first_block, LOX_IR_ALLOCATOR(ci->lox_ir));
    struct ssa_name entered_loop_flag_initialized = alloc_ssa_name_lox_ir(ci->lox_ir, 0, "EnteredLoop", ci->lox_ir->first_block,
            create_lox_ir_type(LOX_IR_TYPE_NATIVE_BOOLEAN, LOX_IR_ALLOCATOR(ci->lox_ir)));
    int entered_loop_flag_local_num = entered_loop_flag_initialized.value.local_number;
    define_entered_loop_flag->ssa_name = entered_loop_flag_initialized;
    define_entered_loop_flag->value = &create_lox_ir_const_node(0, LOX_IR_TYPE_NATIVE_BOOLEAN, NULL, LOX_IR_ALLOCATOR(ci->lox_ir))->data;
    add_last_control_node_block_lox_ir(ci->lox_ir, ci->lox_ir->first_block, &define_entered_loop_flag->control);

    //Set entered loop flag to true when loop entered
    struct lox_ir_control_define_ssa_name_node * set_true_entered_loop_flag =  ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME,
            struct lox_ir_control_define_ssa_name_node, blocK_definition_uncasted, LOX_IR_ALLOCATOR(ci->lox_ir));
    struct ssa_name entered_loop_flag_modified = alloc_ssa_version_lox_ir(ci->lox_ir, entered_loop_flag_initialized.value.local_number);
    set_true_entered_loop_flag->ssa_name = entered_loop_flag_modified;
    set_true_entered_loop_flag->value = &create_lox_ir_const_node(1, LOX_IR_TYPE_NATIVE_BOOLEAN, NULL, LOX_IR_ALLOCATOR(ci->lox_ir))->data;
    add_first_control_node_block_lox_ir(ci->lox_ir, blocK_definition_uncasted, &set_true_entered_loop_flag->control);

    //Create condition on loop entered flag
    struct lox_ir_block * check_loop_flag_condition_block = alloc_lox_ir_block(LOX_IR_ALLOCATOR(ci->lox_ir));
    struct lox_ir_block * loop_entered_cast_block = alloc_lox_ir_block(LOX_IR_ALLOCATOR(ci->lox_ir));
    check_loop_flag_condition_block->type_next = TYPE_NEXT_LOX_IR_BLOCK_BRANCH;
    check_loop_flag_condition_block->next_as.branch.false_branch = block_uses_casted;
    check_loop_flag_condition_block->next_as.branch.true_branch = loop_entered_cast_block;
    loop_entered_cast_block->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
    loop_entered_cast_block->next_as.next = block_uses_casted;

    //First phi-resolve the entered_loop_flag_
    struct lox_ir_data_phi_node * phi_loop_entered_flag_condition = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_PHI,
            struct lox_ir_data_phi_node, NULL, LOX_IR_ALLOCATOR(ci->lox_ir));
    phi_loop_entered_flag_condition->local_number = entered_loop_flag_local_num;
    init_u64_set(&phi_loop_entered_flag_condition->ssa_versions, LOX_IR_ALLOCATOR(ci->lox_ir));
    add_u64_set(&phi_loop_entered_flag_condition->ssa_versions, entered_loop_flag_modified.value.version);
    add_u64_set(&phi_loop_entered_flag_condition->ssa_versions, entered_loop_flag_initialized.value.version);
    phi_loop_entered_flag_condition->data.produced_type = create_lox_ir_type(LOX_IR_TYPE_NATIVE_BOOLEAN, LOX_IR_ALLOCATOR(ci->lox_ir));
    struct lox_ir_control_define_ssa_name_node * get_loop_entered_in_condition = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME,
            struct lox_ir_control_define_ssa_name_node, check_loop_flag_condition_block, LOX_IR_ALLOCATOR(ci->lox_ir));
    struct ssa_name loop_entered_flag_phi_resolved = alloc_ssa_version_lox_ir(ci->lox_ir, entered_loop_flag_local_num);
    get_loop_entered_in_condition->value = &phi_loop_entered_flag_condition->data;
    get_loop_entered_in_condition->ssa_name = loop_entered_flag_phi_resolved;

    //Then add cond jump node, to check wether the phi-resolved entered loop flag is true
    struct lox_ir_control_conditional_jump_node * jump_if_loop_flag_condition_is_true = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP,
            struct lox_ir_control_conditional_jump_node, check_loop_flag_condition_block, LOX_IR_ALLOCATOR(ci->lox_ir));
    struct lox_ir_data_binary_node * condition_loop_flag_condition_is_true = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_BINARY,
            struct lox_ir_data_binary_node, NULL, LOX_IR_ALLOCATOR(ci->lox_ir));
    struct lox_ir_data_get_ssa_name_node * get_loop_flag_condition = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GET_SSA_NAME,
            struct lox_ir_data_get_ssa_name_node, NULL, LOX_IR_ALLOCATOR(ci->lox_ir));
    condition_loop_flag_condition_is_true->data.produced_type = create_lox_ir_type(LOX_IR_TYPE_NATIVE_BOOLEAN, LOX_IR_ALLOCATOR(ci->lox_ir));
    get_loop_flag_condition->data.produced_type = create_lox_ir_type(LOX_IR_TYPE_NATIVE_BOOLEAN, LOX_IR_ALLOCATOR(ci->lox_ir));
    get_loop_flag_condition->ssa_name = loop_entered_flag_phi_resolved;
    condition_loop_flag_condition_is_true->operator = OP_EQUAL;
    condition_loop_flag_condition_is_true->left = &get_loop_flag_condition->data;
    condition_loop_flag_condition_is_true->right = &create_lox_ir_const_node(1, LOX_IR_TYPE_NATIVE_BOOLEAN, NULL, LOX_IR_ALLOCATOR(ci->lox_ir))->data;
    jump_if_loop_flag_condition_is_true->condition = &condition_loop_flag_condition_is_true->data;

    add_first_control_node_block_lox_ir(ci->lox_ir, check_loop_flag_condition_block, &jump_if_loop_flag_condition_is_true->control);
    add_first_control_node_block_lox_ir(ci->lox_ir, check_loop_flag_condition_block, &get_loop_entered_in_condition->control);

    return check_loop_flag_condition_block;
}

static void insert_cast_node_and_replace_phi(
        struct ci * ci,
        struct ssa_name ssa_name_to_extract,
        struct lox_ir_block * block_define_casted,
        struct lox_ir_block * block_uses_casted,
        struct lox_ir_control_node * phi_control,
        struct lox_ir_data_phi_node * phi,
        lox_ir_type_t type_to_be_casted
) {
    struct ssa_name casted_ssa_name = alloc_ssa_version_lox_ir(ci->lox_ir, ssa_name_to_extract.value.local_number);
    struct lox_ir_control_define_ssa_name_node * define_casted = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME, struct lox_ir_control_define_ssa_name_node, block_define_casted, LOX_IR_ALLOCATOR(ci->lox_ir)
    );
    struct lox_ir_data_get_ssa_name_node * get_uncasted_ssa_name_node = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, LOX_IR_ALLOCATOR(ci->lox_ir)
    );

    //Get uncasted ssa name (a1 in the example)
    get_uncasted_ssa_name_node->data.produced_type = clone_lox_ir_type(get_type_by_ssa_name_lox_ir(ci->lox_ir, block_uses_casted,
        ssa_name_to_extract), LOX_IR_ALLOCATOR(ci->lox_ir));
    get_uncasted_ssa_name_node->ssa_name = ssa_name_to_extract;
    add_ssa_name_use_lox_ir(ci->lox_ir, ssa_name_to_extract, &define_casted->control);

    //Define casted ssa name (a3 = cast(a1))
    define_casted->ssa_name = casted_ssa_name;
    define_casted->value = &get_uncasted_ssa_name_node->data;
    add_last_control_node_block_lox_ir(ci->lox_ir, block_define_casted, &define_casted->control);
    insert_cast_node(ci, define_casted->value, (void**) &define_casted->value, type_to_be_casted);

    //Added casted ssa name in phi (a2 = phi(a0, a3))
    remove_u64_set(&phi->ssa_versions, ssa_name_to_extract.value.version);
    add_u64_set(&phi->ssa_versions, casted_ssa_name.value.version);
    remove_ssa_name_use_lox_ir(ci->lox_ir, ssa_name_to_extract, phi_control);
    add_ssa_name_use_lox_ir(ci->lox_ir, casted_ssa_name, phi_control);

    on_ssa_name_def_moved_lox_ir(ci->lox_ir, GET_DEFINED_SSA_NAME(phi_control));
    on_ssa_name_def_moved_lox_ir(ci->lox_ir, casted_ssa_name);
}

static struct lox_ir_block * get_block_same_nested_loop_body(
        struct lox_ir_block * start,
        int target_loop_body
) {
    if (start->nested_loop_body == target_loop_body) {
        return start;
    }
    //TODO Asssert that start->nested_loop_body > target_loop_body

    struct lox_ir_block * current = start;

    for (int i = 0; i < abs(start->nested_loop_body - target_loop_body); i++) {
        current = current->loop_condition_block;
    }

    return current;
}

static struct u64_set calculate_predecessors_extract_cast_from_phi(
        struct ci * ci,
        struct ssa_name uncasted_name,
        struct lox_ir_block * block_uses_unscasted
) {
    struct lox_ir_block * block_definition_uncasted = ((struct lox_ir_control_node *) get_u64_hash_table(
            &ci->lox_ir->definitions_by_ssa_name, uncasted_name.u16))->block;

    struct lox_ir_block * possible_paths_lookup_start = block_definition_uncasted;
    struct lox_ir_block * possible_paths_lookup_end = block_uses_unscasted;
    if (block_definition_uncasted->nested_loop_body > block_uses_unscasted->nested_loop_body) {
        possible_paths_lookup_start = get_block_same_nested_loop_body(block_definition_uncasted, block_uses_unscasted->nested_loop_body);
    }
    struct u64_set all_possible_block_paths = get_all_block_paths_to_block_set_lox_ir(ci->lox_ir,
            possible_paths_lookup_start, possible_paths_lookup_end, &ci->ci_allocator.lox_allocator);

    struct u64_set predecessors_block_uses_unscasted = clone_u64_set(&block_uses_unscasted->predecesors,
            LOX_IR_ALLOCATOR(ci->lox_ir));

    intersection_u64_set(&predecessors_block_uses_unscasted, all_possible_block_paths);

    return predecessors_block_uses_unscasted;
}

static bool is_ssa_name_lox(struct ci * ci, struct lox_ir_block * block, struct ssa_name ssa_name) {
    struct lox_ir_type * type = get_type_by_ssa_name_lox_ir(ci->lox_ir, block, ssa_name);
    return type->type == LOX_IR_TYPE_F64 || is_native_lox_ir_type(type->type);
}

static lox_ir_type_t get_type_by_ssa_name(struct ci * ci, struct lox_ir_control_node * control, struct ssa_name name) {
    struct lox_ir_type * type = get_type_by_ssa_name_lox_ir(ci->lox_ir, control->block, name);
    return type->type;
}

static bool is_ssa_name_required_to_have_lox_type(struct ssa_name ssa_name, struct lox_ir_control_node * ssa_name_use) {
    struct stack_list pending;
    init_stack_list(&pending, NATIVE_LOX_ALLOCATOR());
    struct u64_set children = get_children_lox_ir_control(ssa_name_use);
    push_set_stack_list(&pending, children);
    free_u64_set(&children);
    bool ssa_name_required_to_have_lox_type = true;

    while (!is_empty_stack_list(&pending)) {
        struct lox_ir_data_node * current_children = *((struct lox_ir_data_node **) pop_stack_list(&pending));

        if (current_children->type == LOX_IR_DATA_NODE_GET_STRUCT_FIELD
            || current_children->type == LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT) {
            ssa_name_required_to_have_lox_type = false;
            //We won't scan the current_children's children

        } else if (current_children->type == LOX_IR_DATA_NODE_GET_SSA_NAME
            && GET_USED_SSA_NAME(current_children).u16 == ssa_name.u16) {
            ssa_name_required_to_have_lox_type = true;
            break;

        } else {
            struct u64_set child_of_current_children = get_children_lox_ir_data_node(current_children, NATIVE_LOX_ALLOCATOR());
            push_set_stack_list(&pending, child_of_current_children);
            free_u64_set(&child_of_current_children);
        }
    }

    free_stack_list(&pending);

    return ssa_name_required_to_have_lox_type;
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
            struct ssa_name defined_ssa_name = define->ssa_name;
            struct u64_set * uses_ssa_name = get_u64_hash_table(&ci->lox_ir->node_uses_by_ssa_name, defined_ssa_name.u16);
            if(uses_ssa_name == NULL){
                return false;
            }

            bool some_use_requires_lox_type_as_input = false;

            FOR_EACH_U64_SET_VALUE(*uses_ssa_name, struct lox_ir_control_node *, node_uses_ssa_name) {

                if (((node_uses_ssa_name->type == LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT && is_marked_as_escaped_lox_ir_control(node_uses_ssa_name)) ||
                   (node_uses_ssa_name->type == LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD && is_marked_as_escaped_lox_ir_control(node_uses_ssa_name)) ||
                   node_uses_ssa_name->type == LOX_IR_CONTORL_NODE_SET_GLOBAL ||
                   node_uses_ssa_name->type == LOX_IR_CONTROL_NODE_RETURN)
                   && is_ssa_name_required_to_have_lox_type(defined_ssa_name, node_uses_ssa_name)) {
                    some_use_requires_lox_type_as_input = true;
                    break;
                }
            }

            return some_use_requires_lox_type_as_input;
        }
    }
}

static bool can_process(struct ci * ci, struct lox_ir_block * block) {
    if (contains_u64_set(&ci->processed_blocks, (uint64_t) block)) {
        return false;
    }

    bool can_process = true;
    FOR_EACH_U64_SET_VALUE(block->predecesors, uint64_t, current_predecessor) {
        if(!contains_u64_set(&ci->processed_blocks, current_predecessor)){
            can_process = false;
            break;
        }
    }

    return can_process;
}

static void update_prev_parent_produced_type(struct ci * ci, struct lox_ir_data_node * data_node) {
    switch (data_node->type) {
        case LOX_IR_DATA_NODE_BINARY: {
            struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) data_node;
            binary->data.produced_type->type = calculate_type_produced_by_binary(binary);
            break;
        }
        case LOX_IR_DATA_NODE_UNARY: {
            struct lox_ir_data_unary_node * unary = (struct lox_ir_data_unary_node *) data_node;
            unary->data.produced_type->type = unary->operand->produced_type->type;
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            data_node->produced_type->type = LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE;
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            data_node->produced_type->type = LOX_IR_TYPE_NATIVE_ARRAY;
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            data_node->produced_type->type = LOX_IR_TYPE_NATIVE_I64;
            break;
        }
        //The type is inserted when creating a guard_node, so we don't have to anything here
        case LOX_IR_DATA_NODE_GUARD: break;
        default: break;
    }
}

static struct ci * alloc_cast_insertion(struct lox_ir * lox_ir) {
    struct ci * ci =  NATIVE_LOX_MALLOC(sizeof(struct ci));
    ci->lox_ir = lox_ir;
    struct arena arena;
    init_arena(&arena);
    ci->ci_allocator = to_lox_allocator_arena(arena);
    init_u64_set(&ci->processed_blocks, &ci->ci_allocator.lox_allocator);
    init_u64_set(&ci->processed_ssa_names, &ci->ci_allocator.lox_allocator);

    return ci;
}

static void free_casting_insertion(struct ci * ci) {
    free_arena(&ci->ci_allocator.arena);
    NATIVE_LOX_FREE(ci);
}

static lox_ir_type_t union_binary_format_lox_type(lox_ir_type_t left, lox_ir_type_t right) {
    if (left == LOX_IR_TYPE_LOX_ANY || right == LOX_IR_TYPE_LOX_ANY) {
        return LOX_IR_TYPE_LOX_ANY;
    }
    if (is_native_lox_ir_type(left) && is_native_lox_ir_type(right)) {
        return left;
    }
    //At this point some of the types have lox binary format
    if (left == LOX_IR_TYPE_F64 && right == LOX_IR_TYPE_LOX_I64) {
        return left;
    }
    if (left == LOX_IR_TYPE_LOX_I64 && right == LOX_IR_TYPE_F64) {
        return right;
    }
    //Return any of the left or right types since both have the same binary format
    return left;
}