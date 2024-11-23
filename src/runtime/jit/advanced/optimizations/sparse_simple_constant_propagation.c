#include "sparse_simple_constant_propagation.h"

//TOP > SEMILATTICE_BOTTOM > CONSTANT
//Don't change the order, the higher the index, the stronger when calculating binary operations
typedef enum {
    SEMILATTICE_CONSTANT, //Constant value
    SEMILATTICE_BOTTOM, //Multiple values
    SEMILATTICE_TOP, //Unknown value
} semilattice_type_t;

struct semilattice_value {
    semilattice_type_t type;
    struct u64_set values; //Set of lox_value_t
};

struct sparse_simple_constant_propagation {
    struct stack_list pending;
    struct u64_hash_table semilattice_by_ssa_name;
    struct ssa_ir * ssa_ir;
};

struct constant_rewrite {
    struct ssa_data_node * node;
    struct semilattice_value semilattice;
};

extern lox_value_t addition_lox(lox_value_t a, lox_value_t b);
extern void runtime_panic(char * format, ...);

static void initialization(struct sparse_simple_constant_propagation * sscp);
static void propagation(struct sparse_simple_constant_propagation * sscp);

static struct semilattice_value * get_semilattice_initialization_from_data(struct ssa_data_node *current_data_node);
static struct semilattice_value * get_semilattice_propagation_from_data(struct sparse_simple_constant_propagation *, struct ssa_data_node *);
struct sparse_simple_constant_propagation * alloc_sparse_constant_propagation(struct ssa_ir *);
static void free_sparse_constant_propagation(struct sparse_simple_constant_propagation *);
static lox_value_type calculate_binary_lox(lox_value_t, lox_value_t, bytecode_t operator);
static lox_value_type calculate_unary_lox(lox_value_t, ssa_unary_operator_type_t operator);
static void rewrite_graph_as_constant(struct ssa_data_node * old_node, struct ssa_data_node ** parent_ptr, lox_value_t constant);
static struct u64_set_iterator node_uses_by_ssa_name_iterator(struct u64_hash_table, struct ssa_name);
static void rewrite_constant_expressions(struct sparse_simple_constant_propagation *, struct ssa_control_node *);
static struct semilattice_value * alloc_semilatttice(semilattice_type_t, struct u64_set);
static struct semilattice_value * alloc_single_const_value_semilattice(lox_value_t value);
static struct semilattice_value * alloc_multiple_const_values_semilattice(struct u64_set values);
static struct semilattice_value * alloc_top_semilatttice();
static struct semilattice_value * alloc_bottom_semilatttice();
static struct semilattice_value * get_semilattice_phi(struct sparse_simple_constant_propagation *, struct ssa_data_phi_node *);
static struct semilattice_value * join_semilattice(struct semilattice_value *, struct semilattice_value *, bytecode_t);
static struct constant_rewrite * create_constant_rewrite_from_result(struct ssa_data_node *data_node, struct u64_set possible_values);
static struct constant_rewrite * alloc_constant_rewrite(struct ssa_data_node *, struct semilattice_value *);

void perform_sparse_simple_constant_propagation(struct ssa_ir * ssa_ir) {
    struct sparse_simple_constant_propagation * sscp = alloc_sparse_constant_propagation(ssa_ir);

    initialization(sscp);
    propagation(sscp);

    free_sparse_constant_propagation(sscp);
}

static void propagation(struct sparse_simple_constant_propagation * sscp) {
    while (!is_empty_stack_list(&sscp->pending)) {
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64((uint64_t) pop_stack_list(&sscp->pending));
        struct u64_set_iterator nodes_uses_ssa_name_it = node_uses_by_ssa_name_iterator(sscp->ssa_ir->node_uses_by_ssa_name, current_ssa_name);

        while (has_next_u64_set_iterator(nodes_uses_ssa_name_it)) {
            struct ssa_control_node * node_uses_ssa_name = (void *) next_u64_set_iterator(&nodes_uses_ssa_name_it);
            rewrite_constant_expressions(sscp, node_uses_ssa_name);

            if (node_uses_ssa_name->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME) {
                struct ssa_control_define_ssa_name_node * define_ssa_name_node = (struct ssa_control_define_ssa_name_node *) node_uses_ssa_name;
                struct ssa_name define_ssa_name = define_ssa_name_node->ssa_name;
                struct semilattice_value * prev_semilattice = get_u64_hash_table(
                        &sscp->semilattice_by_ssa_name, define_ssa_name.u16);

                if (prev_semilattice->type != SEMILATTICE_BOTTOM) {
                    struct semilattice_value * current_semilattice = get_semilattice_propagation_from_data(
                            sscp, define_ssa_name_node->value);
                    put_u64_hash_table(&sscp->semilattice_by_ssa_name, current_ssa_name.u16, current_semilattice);

                    if(current_semilattice->type != prev_semilattice->type){
                        push_stack_list(&sscp->pending, (void *) current_ssa_name.u16);
                    }

                    free(prev_semilattice);
                }
            }
        }
    }
}

//Initializes list of ssa names to work with
//Also rewrites constant expressions like: 1 + 1 -> 2
static void initialization(struct sparse_simple_constant_propagation * sscp) {
    struct u64_hash_table_iterator ssa_names_iterator;
    init_u64_hash_table_iterator(&ssa_names_iterator, sscp->ssa_ir->ssa_definitions_by_ssa_name);

    while (has_next_u64_hash_table_iterator(ssa_names_iterator)) {
        struct u64_hash_table_entry entry = next_u64_hash_table_iterator(&ssa_names_iterator);
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64(entry.key);
        struct ssa_control_define_ssa_name_node * current_definition = entry.value;

        //We rewrite constant expressions
        rewrite_constant_expressions(sscp, entry.value);

        struct semilattice_value * semilattice_value = get_semilattice_initialization_from_data(current_definition->value);
        put_u64_hash_table(&sscp->semilattice_by_ssa_name, current_ssa_name.u16, semilattice_value);

        if (semilattice_value->type != SEMILATTICE_TOP) {
            push_stack_list(&sscp->pending, (void *) current_ssa_name.u16);
        }
    }
}

static struct semilattice_value * get_semilattice_propagation_from_data(
        struct sparse_simple_constant_propagation * sscp,
        struct ssa_data_node * current_node
) {
    switch (current_node->type) {
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) current_node;
            struct semilattice_value * right = get_semilattice_propagation_from_data(sscp, binary->right);
            struct semilattice_value * left = get_semilattice_propagation_from_data(sscp, binary->left);
            return join_semilattice(left, right, binary->operand);
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) current_node;
            return get_semilattice_propagation_from_data(sscp, unary->operand);
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current_node;
            return get_u64_hash_table(&sscp->semilattice_by_ssa_name, get_ssa_name->ssa_name.u16);
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * constant_node = (struct ssa_data_constant_node *) current_node;
            return alloc_single_const_value_semilattice(constant_node->constant_lox_value);
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            return get_semilattice_phi(sscp, (struct ssa_data_phi_node *) current_node);
        }
        //Multiple values:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CALL: {
            return alloc_bottom_semilatttice();
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        default:
            runtime_panic("Unhandled ssa data node type %i in get_semilattice_initialization_from_data() in sparse_simple_constant_propagation.c", current_node->type);
    }
}

static struct semilattice_value * get_semilattice_initialization_from_data(struct ssa_data_node *current_data_node) {
    switch (current_data_node->type) {
        case SSA_DATA_NODE_TYPE_BINARY: {
            return alloc_top_semilatttice();
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            return alloc_top_semilatttice();
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current_data_node;
            if (get_ssa_name->ssa_name.value.version == 0) {
                return alloc_bottom_semilatttice(); //Function parameter, can have multiple values
            } else {
                return alloc_top_semilatttice();
            }
        }
        //Constant value:
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * constant_node = (struct ssa_data_constant_node *) current_data_node;
            return alloc_single_const_value_semilattice(constant_node->constant_lox_value);
        }
        //Unknown value
        case SSA_DATA_NODE_TYPE_PHI: {
            return alloc_top_semilatttice();
        }
        //Multiple values:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CALL: {
            return alloc_bottom_semilatttice();
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        default:
            runtime_panic("Unhandled ssa data node type %i in get_semilattice_initialization_from_data() in sparse_simple_constant_propagation.c", current_data_node->type);
    }
}

struct constant_rewrite * rewrite_constant_expressions_data_node(
        struct sparse_simple_constant_propagation * sscp,
        struct ssa_data_node * current_node
) {
    switch (current_node->type) {
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call_node = (struct ssa_data_function_call_node *) current_node;
            for(int i = 0; i < call_node->n_arguments; i++){
                struct ssa_data_node * current_argument = call_node->arguments[i];
                call_node->arguments[i] = rewrite_constant_expressions_data_node(sscp, current_argument)->node;
            }
            return alloc_constant_rewrite(current_node, alloc_top_semilatttice());
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current_node;
            struct constant_rewrite * right = rewrite_constant_expressions_data_node(sscp, binary_node->right);
            struct constant_rewrite * left = rewrite_constant_expressions_data_node(sscp, binary_node->left);
            binary_node->right = right->node;
            binary_node->left = left->node;

            struct semilattice_value * result = join_semilattice(&left->semilattice, &right->semilattice, binary_node->operand);

            switch (result->type) {
                case SEMILATTICE_CONSTANT:
                    return create_constant_rewrite_from_result(current_node, result->values);
                case SEMILATTICE_BOTTOM:
                    return alloc_constant_rewrite(current_node, alloc_bottom_semilatttice());
                case SEMILATTICE_TOP:
                    return alloc_constant_rewrite(current_node, alloc_top_semilatttice());
            }
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) current_node;
            struct constant_rewrite * unary_operand_node = rewrite_constant_expressions_data_node(sscp, unary_node->operand);
            unary_node->operand = unary_operand_node->node;

            switch (unary_operand_node->semilattice.type) {
                case SEMILATTICE_CONSTANT:
                    struct u64_set new_possible_values;
                    init_u64_set(&new_possible_values);

                    FOR_EACH_U64_SET_VALUE(unary_operand_node->semilattice.values, current_operand) {
                        lox_value_t current_result = calculate_unary_lox(current_operand, unary_node->operator_type);
                        add_u64_set(&new_possible_values, current_result);
                    }

                    return create_constant_rewrite_from_result(current_node, new_possible_values);
                case SEMILATTICE_BOTTOM:
                    return alloc_constant_rewrite(current_node, alloc_bottom_semilatttice());
                case SEMILATTICE_TOP:
                    return alloc_constant_rewrite(current_node, alloc_top_semilatttice());
            }

            break;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current_node;
            struct semilattice_value * semilattice_ssa_name = get_u64_hash_table(&sscp->semilattice_by_ssa_name, get_ssa_name->ssa_name.u16);

            if (semilattice_ssa_name->type == SEMILATTICE_CONSTANT && size_u64_set(semilattice_ssa_name->values) == 1) {
                lox_value_t ssa_value = get_first_value_u64_set(semilattice_ssa_name->values);
                struct ssa_data_constant_node * constant_node = create_ssa_const_node(ssa_value, NULL);
                return alloc_constant_rewrite(&constant_node->data, alloc_single_const_value_semilattice(ssa_value));
            } else {
                return alloc_constant_rewrite(current_node, semilattice_ssa_name);
            }
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * const_node = (struct ssa_data_constant_node *) current_node;
            return alloc_constant_rewrite(current_node, alloc_single_const_value_semilattice(const_node->constant_lox_value));
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            return alloc_constant_rewrite(current_node, alloc_bottom_semilatttice());
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_data_phi_node * phi_node = (struct ssa_data_phi_node *) current_node;
            return current_node;
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            runtime_panic("Illegal get local node in sparse constant sscp ssa optimization");
        }
    }

    return NULL;
}

static void rewrite_constant_expressions_consumer(
        struct ssa_data_node * _,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * extra
) {
    struct sparse_simple_constant_propagation * sscp = extra;
    *parent_child_ptr = rewrite_constant_expressions_data_node(sscp, current_node);
}

static void rewrite_constant_expressions(
        struct sparse_simple_constant_propagation * sscp,
        struct ssa_control_node * current_node
) {
    // We iterate all the ssa_data_nodes in current_node
    for_each_data_node_in_control_node(current_node, sscp, SSA_CONTROL_NODE_OPT_NOT_RECURSIVE,
                                       rewrite_constant_expressions_consumer);
}

static lox_value_type calculate_unary_lox(lox_value_t operand_value, ssa_unary_operator_type_t operator) {
    switch (operator) {
        case UNARY_OPERATION_TYPE_NEGATION: {
            return TO_LOX_VALUE_NUMBER(-AS_NUMBER(operand_value));
        }
        case UNARY_OPERATION_TYPE_NOT: {
            return TO_LOX_VALUE_BOOL(!AS_BOOL(operand_value));
        }
        default:
            runtime_panic("Unhandled unary operator type %i in calculate_unary() in sparse_simple_constant_propagation.c", operator);
    }
}

struct sparse_simple_constant_propagation * alloc_sparse_constant_propagation(struct ssa_ir * ssa_ir) {
    struct sparse_simple_constant_propagation * sscp = malloc(sizeof(struct sparse_simple_constant_propagation));
    init_u64_hash_table(&sscp->semilattice_by_ssa_name);
    init_stack_list(&sscp->pending);
    sscp->ssa_ir = ssa_ir;
    return sscp;
}

static lox_value_type calculate_binary_lox(lox_value_t left, lox_value_t right, bytecode_t operator) {
    switch (operator) {
        case OP_ADD: return addition_lox(left, right);
        case OP_SUB: return TO_LOX_VALUE_NUMBER(AS_NUMBER(left) - AS_NUMBER(right));
        case OP_MUL: return TO_LOX_VALUE_NUMBER(AS_NUMBER(left) * AS_NUMBER(right));
        case OP_DIV: return TO_LOX_VALUE_NUMBER(AS_NUMBER(left) / AS_NUMBER(right));
        case OP_GREATER: return TO_LOX_VALUE_BOOL(AS_NUMBER(left) > AS_NUMBER(right));
        case OP_LESS: return TO_LOX_VALUE_BOOL(AS_NUMBER(left) < AS_NUMBER(right));
        case OP_EQUAL: return TO_LOX_VALUE_BOOL(left == right);
        default: runtime_panic("Unhandled binary operator %i in calculate_binary_lox() in sparse_simple_constant_propagation.c", operator);
    }
}

static void rewrite_graph_as_constant(struct ssa_data_node * old_node, struct ssa_data_node ** parent_ptr, lox_value_t constant) {
    struct ssa_data_constant_node * constant_node = create_ssa_const_node(constant, NULL);
    *parent_ptr = &constant_node->data;
    free_ssa_data_node(old_node);
}

static struct u64_set_iterator node_uses_by_ssa_name_iterator(struct u64_hash_table node_uses_by_ssa_name, struct ssa_name ssa_name) {
    if(contains_u64_hash_table(&node_uses_by_ssa_name, ssa_name.u16)){
        struct u64_set * node_uses_by_ssa_name_set = get_u64_hash_table(&node_uses_by_ssa_name, ssa_name.u16);
        struct u64_set_iterator node_uses_by_ssa_name_iterator;
        init_u64_set_iterator(&node_uses_by_ssa_name_iterator, *node_uses_by_ssa_name_set);
        return node_uses_by_ssa_name_iterator;
    } else {
        //Return emtpy iterator
        struct u64_set empty_set;
        init_u64_set(&empty_set);
        struct u64_set_iterator node_uses_by_ssa_name_empty_iterator;
        init_u64_set_iterator(&node_uses_by_ssa_name_empty_iterator, empty_set);
        return node_uses_by_ssa_name_empty_iterator;
    }
}

static struct semilattice_value * alloc_multiple_const_values_semilattice(struct u64_set values) {
    struct semilattice_value * semilattice = malloc(sizeof(struct semilattice_value));
    init_u64_set(&semilattice->values);
    semilattice->type = SEMILATTICE_CONSTANT;
    union_u64_set(&semilattice->values, values);
    return semilattice;
}

static struct semilattice_value * alloc_single_const_value_semilattice(lox_value_t value) {
    struct semilattice_value * semilattice = malloc(sizeof(struct semilattice_value));
    init_u64_set(&semilattice->values);
    semilattice->type = SEMILATTICE_CONSTANT;
    add_u64_set(&semilattice->values, value);
    return semilattice;
}

static struct semilattice_value * alloc_top_semilatttice() {
    struct semilattice_value * semilattice = malloc(sizeof(struct semilattice_value));
    init_u64_set(&semilattice->values);
    semilattice->type = SEMILATTICE_TOP;
    return semilattice;
}

static struct semilattice_value * alloc_bottom_semilatttice() {
    struct semilattice_value * semilattice = malloc(sizeof(struct semilattice_value));
    init_u64_set(&semilattice->values);
    semilattice->type = SEMILATTICE_BOTTOM;
    return semilattice;
}

static void free_sparse_constant_propagation(struct sparse_simple_constant_propagation * sscp) {
    free_stack_list(&sscp->pending);
    struct u64_hash_table_iterator iterator;
    init_u64_hash_table_iterator(&iterator, sscp->semilattice_by_ssa_name);
    while(has_next_u64_hash_table_iterator(iterator)){
        struct semilattice_value * semilattice = next_u64_hash_table_iterator(&iterator).value;
        free(semilattice);
    }
    free(sscp);
}

static struct semilattice_value * alloc_semilatttice(semilattice_type_t type, struct u64_set values) {
    struct semilattice_value * semilattice = malloc(sizeof(struct semilattice_value));
    semilattice->values = values;
    semilattice->type = type;
    return semilattice;
}

static struct semilattice_value * get_semilattice_phi(
        struct sparse_simple_constant_propagation * sscp,
        struct ssa_data_phi_node * phi_node
) {
    semilattice_type_t final_semilattice_type = SEMILATTICE_TOP;
    bool top_value_found = false;
    struct u64_set final_values;
    init_u64_set(&final_values);

    FOR_EACH_VERSION_IN_PHI_NODE(phi_node, current_name) {
        struct semilattice_value * current_semilatice = get_u64_hash_table(&sscp->semilattice_by_ssa_name, current_name.u16);
        union_u64_set(&final_values, current_semilatice->values);

        if(final_semilattice_type == SEMILATTICE_BOTTOM) {
            continue;
        } else if(current_semilatice->type == SEMILATTICE_BOTTOM) {
            final_semilattice_type = SEMILATTICE_BOTTOM;
        } else if(current_semilatice->type == SEMILATTICE_TOP){
            final_semilattice_type = SEMILATTICE_TOP;
            top_value_found = true;
        } else if(current_semilatice->type == SEMILATTICE_CONSTANT && !top_value_found){
            final_semilattice_type = SEMILATTICE_CONSTANT;
        }
    }

    return alloc_semilatttice(final_semilattice_type, final_values);
}

static struct semilattice_value * join_semilattice(
        struct semilattice_value * left,
        struct semilattice_value * right,
        bytecode_t operator
) {
    if (left->type == SEMILATTICE_BOTTOM || right->type == SEMILATTICE_BOTTOM) {
        return alloc_top_semilatttice();
    }

    struct u64_set final_values;
    init_u64_set(&final_values);

    if(left->type == SEMILATTICE_TOP || right->type == SEMILATTICE_TOP){
        union_u64_set(&final_values, right->values);
        union_u64_set(&final_values, left->values);
        return alloc_semilatttice(SEMILATTICE_TOP, final_values);
    }

    //Constant & Constant. Compute possible final_values
    struct u64_set_iterator left_iterator;
    init_u64_set_iterator(&left_iterator, left->values);

    while(has_next_u64_set_iterator(left_iterator)){
        struct u64_set_iterator right_iterator;
        init_u64_set_iterator(&right_iterator, right->values);

        while(has_next_u64_set_iterator(right_iterator)){
            lox_value_t right_value = next_u64_set_iterator(&right_iterator);
            lox_value_t left_value = next_u64_set_iterator(&left_iterator);

            lox_value_t result = calculate_binary_lox(left_value, right_value, operator);
            add_u64_set(&final_values, result);
        }
    }

    return alloc_semilatttice(SEMILATTICE_CONSTANT, final_values);
}

static struct constant_rewrite * create_constant_rewrite_from_result(struct ssa_data_node * data_node, struct u64_set possible_values) {
    if(size_u64_set(possible_values) == 1){
        lox_value_t value = get_first_value_u64_set(possible_values);
        return alloc_constant_rewrite(&create_ssa_const_node(value, NULL)->data,
                                      alloc_single_const_value_semilattice(value));
    } else {
        return alloc_constant_rewrite(data_node, alloc_multiple_const_values_semilattice(possible_values));
    }
}