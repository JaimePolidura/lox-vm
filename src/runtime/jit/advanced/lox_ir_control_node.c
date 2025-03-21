#include "lox_ir_control_node.h"

void * allocate_lox_ir_control(
        lox_ir_control_node_type type,
        size_t struct_size_bytes,
        struct lox_ir_block * block,
        struct lox_allocator * allocator
) {
    struct lox_ir_control_node * control_node = LOX_MALLOC(allocator, struct_size_bytes);
    memset(control_node, 0, struct_size_bytes);
    control_node->block = block;
    control_node->type = type;

    if (control_node->type == LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT) {
        ((struct lox_ir_control_set_array_element_node *) control_node)->requires_range_check = true;
    }

    return control_node;
}

bool for_each_data_node_in_lox_ir_control(
        struct lox_ir_control_node * control_node,
        void * extra,
        long options,
        lox_ir_data_node_consumer_t consumer
) {
    struct u64_set children = get_children_lox_ir_control(control_node);
    bool all_nodes_returned_keep_scanning = true;
    
    FOR_EACH_U64_SET_VALUE(children, child_parent_field_ptr_u64) {
        struct lox_ir_data_node ** child_parent_field_ptr = (struct lox_ir_data_node **) child_parent_field_ptr_u64;
        struct lox_ir_data_node * child = *child_parent_field_ptr;

        all_nodes_returned_keep_scanning &= for_each_lox_ir_data_node(child, (void **) child_parent_field_ptr, extra,
            options, consumer);
    }

    return all_nodes_returned_keep_scanning;
}

struct u64_set get_children_lox_ir_control(struct lox_ir_control_node * control_node) {
    struct u64_set children;
    init_u64_set(&children, NATIVE_LOX_ALLOCATOR());

    switch (control_node->type) {
        case LOX_IR_CONTROL_NODE_DATA: {
            struct lox_ir_control_data_node * data_node = (struct lox_ir_control_data_node *) control_node;
            add_u64_set(&children, (uint64_t) &data_node->data);
            break;
        }
        case LOX_IR_CONTROL_NODE_RETURN: {
            struct lox_ir_control_return_node * return_node = (struct lox_ir_control_return_node *) control_node;
            add_u64_set(&children, (uint64_t) &return_node->data);
            break;
        }
        case LOX_IR_CONTROL_NODE_PRINT: {
            struct lox_ir_control_print_node * print_node = (struct lox_ir_control_print_node *) control_node;
            add_u64_set(&children, (uint64_t) &print_node->data);
            break;
        }
        case LOX_IR_CONTORL_NODE_SET_GLOBAL: {
            struct lox_ir_control_set_global_node * set_global = (struct lox_ir_control_set_global_node *) control_node;
            add_u64_set(&children, (uint64_t) &set_global->value_node);
            break;
        }
        case LOX_IR_CONTORL_NODE_SET_LOCAL: {
            struct lox_ir_control_set_local_node * set_local = (struct lox_ir_control_set_local_node *) control_node;
            add_u64_set(&children, (uint64_t) &set_local->new_local_value);
            break;
        }
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD: {
            struct lox_ir_control_set_struct_field_node * set_struct_field = (struct lox_ir_control_set_struct_field_node *) control_node;
            add_u64_set(&children, (uint64_t) &set_struct_field->instance);
            add_u64_set(&children, (uint64_t) &set_struct_field->new_field_value);
            break;
        }
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT: {
            struct lox_ir_control_set_array_element_node * set_array_element = (struct lox_ir_control_set_array_element_node *) control_node;
            add_u64_set(&children, (uint64_t) &set_array_element->new_element_value);
            add_u64_set(&children, (uint64_t) &set_array_element->array);
            add_u64_set(&children, (uint64_t) &set_array_element->index);
            break;
        }
        case LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP: {
            struct lox_ir_control_conditional_jump_node * cond_jump = (struct lox_ir_control_conditional_jump_node *) control_node;
            add_u64_set(&children, (uint64_t) &cond_jump->condition);
            break;
        }
        case LOX_IR_CONTROL_NODE_GUARD: {
            struct lox_ir_control_guard_node * guard = (struct lox_ir_control_guard_node *) control_node;
            add_u64_set(&children, (uint64_t) &guard->guard.value);
            break;
        }
        case LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME: {
            struct lox_ir_control_define_ssa_name_node * define_ssa_name = (struct lox_ir_control_define_ssa_name_node *) control_node;
            add_u64_set(&children, (uint64_t) &define_ssa_name->value);
            break;
        }
        case LOX_IR_CONTROL_NODE_SET_V_REGISTER:  {
            struct lox_ir_control_set_v_register_node * set_v_reg = (struct lox_ir_control_set_v_register_node *) control_node;
            add_u64_set(&children, (uint64_t) &set_v_reg->value);
            break;
        }

        case LOX_IR_CONTROL_NODE_ENTER_MONITOR:
        case LOX_IR_CONTROL_NODE_EXIT_MONITOR:
        case LOX_IR_CONTROL_NODE_LOOP_JUMP:
            break;
    }

    return children;
}

static bool get_used_ssa_names_lox_ir_control_consumer(
        struct lox_ir_data_node * __,
        void ** _,
        struct lox_ir_data_node * current_data_node,
        void * extra
) {
    struct u64_set * used_ssa_names = extra;
    struct u64_set used_ssa_names_in_current_data_node = get_used_ssa_names_lox_ir_data_node(
            current_data_node, NATIVE_LOX_ALLOCATOR()
    );
    union_u64_set(used_ssa_names, used_ssa_names_in_current_data_node);
    free_u64_set(&used_ssa_names_in_current_data_node);

    //Don't keep scanning from this control, because we have already scanned al sub datanodes
    //in get_used_ssa_names_lox_ir_data_node()
    return false;
}

struct u64_set get_used_ssa_names_lox_ir_control(struct lox_ir_control_node * control_node, struct lox_allocator * allocator) {
    struct u64_set used_ssa_names;
    init_u64_set(&used_ssa_names, allocator);

    for_each_data_node_in_lox_ir_control(
            control_node,
            &used_ssa_names,
            LOX_IR_DATA_NODE_OPT_PRE_ORDER,
            &get_used_ssa_names_lox_ir_control_consumer
    );

    return used_ssa_names;
}

bool is_marked_as_escaped_lox_ir_control(struct lox_ir_control_node * node) {
    switch (node->type) {
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD: {
            struct lox_ir_control_set_struct_field_node * set_struct_field = (struct lox_ir_control_set_struct_field_node *) node;
            return set_struct_field->escapes;
        }
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT: {
            struct lox_ir_control_set_array_element_node * set_arr_element = (struct lox_ir_control_set_array_element_node *) node;
            return set_arr_element->escapes;
        }
        default: {
            return false;
        }
    }
}

void mark_as_escaped_lox_ir_control(struct lox_ir_control_node * node) {
    switch (node->type) {
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD: {
            struct lox_ir_control_set_struct_field_node * set_struct_field = (struct lox_ir_control_set_struct_field_node *) node;
            set_struct_field->escapes = true;
            break;
        }
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT: {
            struct lox_ir_control_set_array_element_node * set_array_element = (struct lox_ir_control_set_array_element_node *) node;
            set_array_element->escapes = true;
            break;
        }
    }
}

bool is_lowered_type_lox_ir_control(struct lox_ir_control_node *node) {
    return node->type == LOX_IR_CONTROL_NODE_LL_MOVE
        || node->type == LOX_IR_CONTROL_NODE_LL_BINARY
        || node->type == LOX_IR_CONTROL_NODE_LL_COMPARATION
        || node->type == LOX_IR_CONTROL_NODE_LL_UNARY
        || node->type == LOX_IR_CONTROL_NODE_LL_RETURN
        || node->type == LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL
        || node->type == LOX_IR_CONTROL_NODE_LL_COND_FUNCTION_CALL;
}

bool is_define_phi_lox_ir_control(struct lox_ir_control_node * node) {
    return node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME
        && (GET_DEFINED_SSA_NAME_VALUE(node)->type == LOX_IR_DATA_NODE_PHI
            || (GET_DEFINED_SSA_NAME_VALUE(node)->type == LOX_IR_DATA_NODE_CAST
                && ((struct lox_ir_data_cast_node *) GET_DEFINED_SSA_NAME_VALUE(node))->to_cast->type == LOX_IR_DATA_NODE_PHI));
}

struct lox_ir_data_phi_node * get_defined_phi_lox_ir_control(struct lox_ir_control_node * node) {
    struct lox_ir_control_define_ssa_name_node * definition = (struct lox_ir_control_define_ssa_name_node *) node;

    if (definition->value->type == LOX_IR_DATA_NODE_PHI) {
        return (struct lox_ir_data_phi_node *) definition->value;
    }

    struct lox_ir_data_cast_node * cast_node = (struct lox_ir_data_cast_node *) definition->value;
    return (struct lox_ir_data_phi_node *) cast_node->to_cast;
}