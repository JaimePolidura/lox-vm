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
    
    FOR_EACH_U64_SET_VALUE(children, struct lox_ir_data_node **, child_parent_field_ptr) {
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
        case LOX_IR_CONTROL_NODE_ENTER_MONITOR:
        case LOX_IR_CONTROL_NODE_EXIT_MONITOR:
        case LOX_IR_CONTROL_NODE_LOOP_JUMP:
        case LOX_IR_CONTROL_NODE_LL_MOVE:
        case LOX_IR_CONTROL_NODE_LL_BINARY:
        case LOX_IR_CONTROL_NODE_LL_COMPARATION:
        case LOX_IR_CONTROL_NODE_LL_UNARY:
        case LOX_IR_CONTROL_NODE_LL_RETURN:
        case LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL:
        case LOX_IR_CONTROL_NODE_LL_COND_FUNCTION_CALL:
            break;
        default:
            lox_assert_failed("lox_ir_control_node.c.c::get_children_lox_ir_control", "Uknown control node type %i", control_node->type);
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

    struct u64_set operands = get_used_ll_operands_lox_ir_control(control_node, allocator);
    FOR_EACH_U64_SET_VALUE(operands, struct lox_ir_ll_operand *, operand) {
        struct u64_set ssa_names_used_in_operand = get_used_v_reg_ssa_name_ll_operand(*operand, allocator);
        union_u64_set(&used_ssa_names, ssa_names_used_in_operand);
    }

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
        default:
            break;
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
    bool is_define_phi_case_1 = node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME &&
            (GET_DEFINED_SSA_NAME_VALUE(node)->type == LOX_IR_DATA_NODE_PHI
                || GET_DEFINED_SSA_NAME_VALUE(node)->type == LOX_IR_DATA_NODE_CAST);

    bool is_define_phi_case_2 = node->type == LOX_IR_CONTROL_NODE_LL_MOVE &&
            ((struct lox_ir_control_ll_move *) node)->from.type == LOX_IR_LL_OPERAND_PHI_V_REGISTER;

    return is_define_phi_case_1 || is_define_phi_case_2;
}

struct lox_ir_data_phi_node * get_defined_phi_lox_ir_control(struct lox_ir_control_node * node) {
    struct lox_ir_control_define_ssa_name_node * definition = (struct lox_ir_control_define_ssa_name_node *) node;

    if (definition->value->type == LOX_IR_DATA_NODE_PHI) {
        return (struct lox_ir_data_phi_node *) definition->value;
    }

    struct lox_ir_data_cast_node * cast_node = (struct lox_ir_data_cast_node *) definition->value;
    return (struct lox_ir_data_phi_node *) cast_node->to_cast;
}

struct u64_set get_names_defined_phi_lox_ir_control(
        struct lox_ir_control_node * node,
        struct lox_allocator * allocator
) {
    lox_assert(node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME || node->type == LOX_IR_CONTROL_NODE_LL_MOVE,
               "lox_ir_control_node::get_names_defined_phi_lox_ir_control", "Types can only be move or define");

    struct u64_set ssa_names;
    init_u64_set(&ssa_names, allocator);

    if (node->type == LOX_IR_CONTROL_NODE_LL_MOVE) {
        struct lox_ir_control_ll_move * move = (struct lox_ir_control_ll_move *) node;
        FOR_EACH_U64_SET_VALUE(move->to.phi_v_register.versions, uint64_t, version) {
            struct ssa_name ssa_name = CREATE_SSA_NAME(move->to.phi_v_register.v_register.ssa_name.value.local_number, version);
            add_u64_set(&ssa_names, ssa_name.u16);
        }

    } else {
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) node;
        struct lox_ir_data_phi_node * phi = (struct lox_ir_data_phi_node *) (define->value->type != LOX_IR_DATA_NODE_PHI ?
            ((struct lox_ir_data_cast_node *) GET_DEFINED_SSA_NAME_VALUE(node))->to_cast :
            define->value);

        FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, ssa_name) {
            add_u64_set(&ssa_names, ssa_name.u16);
        }
    }

    return ssa_names;
}

static void get_used_ll_operands_func_call_lox_ir_control(
        struct u64_set * used_ll_operands,
        struct lox_ir_control_ll_function_call * call
) {
    for (int i = 0; i < call->arguments.in_use; i++) {
        add_u64_set(used_ll_operands, (uint64_t) call->arguments.values[i]);
    }
    if (call->has_return_value) {
        add_u64_set(used_ll_operands, (uint64_t) &call->return_value_v_reg);
    }
}

struct u64_set get_used_ll_operands_lox_ir_control(struct lox_ir_control_node * control_node, struct lox_allocator * allocator) {
    struct u64_set used_ll_operands;
    init_u64_set(&used_ll_operands, allocator);

    switch (control_node->type) {
        case LOX_IR_CONTROL_NODE_LL_MOVE: {
            struct lox_ir_control_ll_move * move = (struct lox_ir_control_ll_move *) control_node;
            add_u64_set(&used_ll_operands, (uint64_t) &move->to);
            add_u64_set(&used_ll_operands, (uint64_t) &move->from);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_BINARY: {
            struct lox_ir_control_ll_binary * binary = (struct lox_ir_control_ll_binary *) control_node;
            add_u64_set(&used_ll_operands, (uint64_t) &binary->left);
            add_u64_set(&used_ll_operands, (uint64_t) &binary->right);
            add_u64_set(&used_ll_operands, (uint64_t) &binary->result);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_COMPARATION: {
            struct lox_ir_control_ll_comparation * cmp = (struct lox_ir_control_ll_comparation *) control_node;
            add_u64_set(&used_ll_operands, (uint64_t) &cmp->left);
            add_u64_set(&used_ll_operands, (uint64_t) &cmp->right);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_UNARY: {
            struct lox_ir_control_ll_unary * unary = (struct lox_ir_control_ll_unary *) control_node;
            add_u64_set(&used_ll_operands, (uint64_t) &unary->operand);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_RETURN: {
            struct lox_ir_control_ll_return * ret = (struct lox_ir_control_ll_return *) control_node;
            if (!ret->empty_return) {
                add_u64_set(&used_ll_operands, (uint64_t) &ret->to_return);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL: {
            struct lox_ir_control_ll_function_call * call = (struct lox_ir_control_ll_function_call *) control_node;
            get_used_ll_operands_func_call_lox_ir_control(&used_ll_operands, call);
            break;
        }
        case LOX_IR_CONTROL_NODE_LL_COND_FUNCTION_CALL: {
            struct lox_ir_control_ll_cond_function_call * cond_call = (struct lox_ir_control_ll_cond_function_call *) control_node;
            get_used_ll_operands_func_call_lox_ir_control(&used_ll_operands, cond_call->call);
            break;
        }
    }

    return used_ll_operands;
}

static void replace_v_reg_ssa_node(
        struct lox_ir_control_node * node,
        struct ssa_name old,
        struct ssa_name new
) {
    struct u64_set used_operands = get_used_ll_operands_lox_ir_control(node, NATIVE_LOX_ALLOCATOR());

    FOR_EACH_U64_SET_VALUE(used_operands, struct lox_ir_ll_operand *, operand_used) {
        switch (operand_used->type) {
            case LOX_IR_LL_OPERAND_REGISTER: {
                if (operand_used->v_register.ssa_name.u16 == old.u16) {
                    operand_used->v_register.ssa_name = new;
                }
                break;
            }
            case LOX_IR_LL_OPERAND_PHI_V_REGISTER: {
                if (operand_used->phi_v_register.v_register.ssa_name.value.local_number == old.value.local_number
                    && contains_u64_set(&operand_used->phi_v_register.versions, old.value.version)) {

                    remove_u64_set(&operand_used->phi_v_register.versions, old.value.version);
                    add_u64_set(&operand_used->phi_v_register.versions, new.value.version);
                }
                //If phi has only one version, we should replace it with LOX_IR_LL_OPERAND_PHI_V_REGISTER operand type
                if (size_u64_set(operand_used->phi_v_register.versions) == 1) {
                    operand_used->type = LOX_IR_LL_OPERAND_PHI_V_REGISTER;
                    struct v_register v_register = operand_used->phi_v_register.v_register;
                    v_register.ssa_name.value.version = new.value.version;
                    operand_used->v_register = v_register;
                }
                break;
            }
            case LOX_IR_LL_OPERAND_MEMORY_ADDRESS: {
                if (operand_used->memory_address.address.ssa_name.u16 == old.u16) {
                    operand_used->memory_address.address.ssa_name = new;
                }
                break;
            }
            default:
                break;
        }
    }

    free_u64_set(&used_operands);
}

void replace_ssa_name_lox_ir_control(
        struct lox_ir_control_node * node,
        struct ssa_name old,
        struct ssa_name new
) {
    //Replace data node ssa names
    FOR_EACH_U64_SET_VALUE(get_children_lox_ir_control(node), struct lox_ir_data_node **,child) {
        replace_ssa_name_lox_ir_data(child, old, new);
    }

    if (node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME && GET_DEFINED_SSA_NAME(node).u16 == old.u16) {
        ((struct lox_ir_control_define_ssa_name_node *) node)->ssa_name = new;
    } else if (is_lowered_type_lox_ir_control(node)) {
        replace_v_reg_ssa_node(node, old, new);
    }
}

struct ssa_name * get_defined_ssa_name_lox_ir_control(struct lox_ir_control_node * control_node) {
    if (control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
        return &GET_DEFINED_SSA_NAME(control_node);
    }
    if (control_node->type == LOX_IR_CONTROL_NODE_LL_MOVE) {
        struct lox_ir_control_ll_move * move = (struct lox_ir_control_ll_move *) control_node;
        if (move->to.type == LOX_IR_LL_OPERAND_REGISTER) {
            return &move->to.v_register.ssa_name;
        }
    }

    return NULL;
}