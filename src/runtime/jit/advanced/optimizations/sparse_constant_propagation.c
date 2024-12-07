#include "sparse_constant_propagation.h"

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

//It stands for sparse constant propagation
struct scp {
    struct stack_list pending;
    struct u64_hash_table semilattice_by_ssa_name;
    struct ssa_ir * ssa_ir;
    struct u64_set removed_ssa_names;

    struct arena_lox_allocator scp_allocator;
};

struct constant_rewrite {
    struct ssa_data_node * node;
    struct semilattice_value * semilattice;
};

extern lox_value_t addition_lox(lox_value_t a, lox_value_t b);
extern void runtime_panic(char * format, ...);

#define GET_SSA_NODES_ALLOCATOR(scp) (&(scp)->ssa_ir->ssa_nodes_allocator_arena.lox_allocator)
#define GET_SCP_ALLOCATOR(scp) &(scp)->scp_allocator.lox_allocator

static void initialization(struct scp *);
static void propagation(struct scp *);

static void remove_death_branch(struct scp *, struct ssa_control_node *);
static struct semilattice_value * get_semilattice_initialization_from_data(struct scp*, struct ssa_data_node *current_data_node);
static struct semilattice_value * get_semilattice_propagation_from_data(struct scp *, struct ssa_data_node *);
struct scp * alloc_sparse_constant_propagation(struct ssa_ir *);
static void free_sparse_constant_propagation(struct scp *);
static lox_value_t calculate_binary_lox(lox_value_t, lox_value_t, bytecode_t operator);
static lox_value_t calculate_unary_lox(lox_value_t, ssa_unary_operator_type_t operator);
static void rewrite_graph_as_constant(struct scp *, struct ssa_data_node * old_node, struct ssa_data_node ** parent_ptr, lox_value_t constant);
static struct u64_set_iterator node_uses_by_ssa_name_iterator(struct scp *, struct u64_hash_table, struct ssa_name);
static void rewrite_constant_expressions_propagation(struct scp *scp, struct ssa_control_node *current_node);
static void rewrite_constant_expressions_initialization(struct scp *scp, struct ssa_control_node *current_node);
static struct semilattice_value * get_semillatice_by_ssa_name(struct scp *, struct ssa_name);

static struct semilattice_value * alloc_semilatttice(struct scp *, semilattice_type_t, struct u64_set);
static struct semilattice_value * alloc_single_const_value_semilattice(struct scp *, lox_value_t value);
static struct semilattice_value * alloc_multiple_const_values_semilattice(struct scp *, struct u64_set values);
static struct semilattice_value * alloc_top_semilatttice(struct scp *);
static struct semilattice_value * alloc_bottom_semilatttice(struct scp *);
static struct semilattice_value * get_semilattice_phi(struct scp *, struct ssa_data_phi_node *);
static struct semilattice_value * join_semilattice(struct scp *, struct semilattice_value *, struct semilattice_value *, bytecode_t);
static struct constant_rewrite * create_constant_rewrite_from_result(struct scp *, struct ssa_data_node *data_node, struct u64_set possible_values);
static struct constant_rewrite * alloc_constant_rewrite(struct scp *, struct ssa_data_node *, struct semilattice_value *);
static void remove_from_ir_removed_ssa_names(struct scp *);

void perform_sparse_constant_propagation(struct ssa_ir * ssa_ir) {
    struct scp * sscp = alloc_sparse_constant_propagation(ssa_ir);

    initialization(sscp);
    propagation(sscp);
    remove_from_ir_removed_ssa_names(sscp);

    free_sparse_constant_propagation(sscp);
}

static void propagation(struct scp * scp) {
    while (!is_empty_stack_list(&scp->pending)) {
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64((uint64_t) pop_stack_list(&scp->pending));
        if(contains_u64_set(&scp->removed_ssa_names, current_ssa_name.u16)){
            continue;
        }

        struct u64_set_iterator nodes_uses_ssa_name_it = node_uses_by_ssa_name_iterator(scp, scp->ssa_ir->node_uses_by_ssa_name, current_ssa_name);
        while (has_next_u64_set_iterator(nodes_uses_ssa_name_it)) {
            struct ssa_control_node * node_uses_ssa_name = (void *) next_u64_set_iterator(&nodes_uses_ssa_name_it);
            rewrite_constant_expressions_propagation(scp, node_uses_ssa_name);

            if (node_uses_ssa_name->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME) {
                struct ssa_control_define_ssa_name_node * define_ssa_name_node = (struct ssa_control_define_ssa_name_node *) node_uses_ssa_name;
                struct ssa_name defined_ssa_name = define_ssa_name_node->ssa_name;
                struct semilattice_value * prev_semilattice = get_semillatice_by_ssa_name(scp, defined_ssa_name);

                if (prev_semilattice->type != SEMILATTICE_BOTTOM) {
                    struct semilattice_value * new_semilattice = get_semilattice_propagation_from_data(scp, define_ssa_name_node->value);
                    put_u64_hash_table(&scp->semilattice_by_ssa_name, defined_ssa_name.u16, new_semilattice);

                    if(new_semilattice->type != prev_semilattice->type){
                        push_stack_list(&scp->pending, (void *) defined_ssa_name.u16);
                    }
                }

            } else if (node_uses_ssa_name->type == SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP &&
                    GET_CONDITION_CONDITIONAL_JUMP_SSA_NODE(node_uses_ssa_name)->type == SSA_DATA_NODE_TYPE_CONSTANT) {
                remove_death_branch(scp, node_uses_ssa_name);
            }
        }
    }
}

//Initializes list of ssa names to work with
//Also rewrites constant expressions like: 1 + 1 -> 2
static void initialization(struct scp * scp) {
    struct u64_hash_table_iterator ssa_names_iterator;
    init_u64_hash_table_iterator(&ssa_names_iterator, scp->ssa_ir->ssa_definitions_by_ssa_name);

    while (has_next_u64_hash_table_iterator(ssa_names_iterator)) {
        struct u64_hash_table_entry entry = next_u64_hash_table_iterator(&ssa_names_iterator);
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64(entry.key);
        struct ssa_control_define_ssa_name_node * current_definition = entry.value;

        //We rewrite constant expressions
        rewrite_constant_expressions_initialization(scp, entry.value);

        struct semilattice_value * semilattice_value = get_semilattice_initialization_from_data(scp, current_definition->value);
        put_u64_hash_table(&scp->semilattice_by_ssa_name, current_ssa_name.u16, semilattice_value);

        if (semilattice_value->type != SEMILATTICE_TOP) {
            push_stack_list(&scp->pending, (void *) current_ssa_name.u16);
        }
    }
}

static void remove_death_branch(struct scp * scp, struct ssa_control_node * branch_node) {
    struct ssa_control_conditional_jump_node * cond_jump = (struct ssa_control_conditional_jump_node *) branch_node;
    bool branch_condition_folded = AS_BOOL(GET_CONST_VALUE_SSA_NODE(cond_jump->condition));
    bool remove_true_branch = !branch_condition_folded;

    struct branch_removed branch_removed = remove_branch_ssa_ir(branch_node->block, remove_true_branch, GET_SCP_ALLOCATOR(scp));

    FOR_EACH_U64_SET_VALUE(branch_removed.ssa_name_definitions_removed, removed_ssa_definition_u64) {
        struct ssa_name removed_ssa_definition = CREATE_SSA_NAME_FROM_U64(removed_ssa_definition_u64);
        struct u64_set * removed_ssa_name_node_uses = get_u64_hash_table(&scp->ssa_ir->node_uses_by_ssa_name, removed_ssa_definition.u16);

        FOR_EACH_U64_SET_VALUE(*removed_ssa_name_node_uses, node_uses_removed_ssa_name_ptr) {
            //Every control_node that uses the removed ssa_node in an extern control_node will use it to define another ssa name, with a phi function:
            //a2 = phi(a0, a1)
            struct ssa_control_define_ssa_name_node * node_uses_ssa_name = (void *) node_uses_removed_ssa_name_ptr;
            if(!contains_u64_set(&branch_removed.blocks_removed, (uint64_t) node_uses_ssa_name->control.block)){
                struct ssa_data_phi_node * phi_node = (struct ssa_data_phi_node *) node_uses_ssa_name->value;
                //Remove ssa name usage from phi control_node
                remove_u64_set(&phi_node->ssa_versions, removed_ssa_definition.value.version);
                //If there is only 1 usage of a ssa name in a phi control_node, replace it with ssa_data_get_ssa_name_node control_node
                if (size_u64_set(phi_node->ssa_versions) == 1) {
                    node_uses_ssa_name->value = (struct ssa_data_node *) ALLOC_SSA_DATA_NODE(
                            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, &scp->ssa_ir->ssa_nodes_allocator_arena.lox_allocator
                    );
                }
                //Remove semilattice of the removed ssa name
                remove_u64_hash_table(&scp->semilattice_by_ssa_name, removed_ssa_definition.u16);
                //Clear node_uses_ssa_name's semilattice values.
                struct semilattice_value * current_semilattice_node_uses = get_semillatice_by_ssa_name(scp, node_uses_ssa_name->ssa_name);
                clear_u64_set(&current_semilattice_node_uses->values);
                //Rescan node_uses_ssa_name: Mark as TOP and push it to the pending stack
                current_semilattice_node_uses->type = SEMILATTICE_TOP;
                push_stack_list(&scp->pending, (void *) node_uses_ssa_name->ssa_name.u16);
                //Mark removed ssa name as removed
                add_u64_set(&scp->removed_ssa_names, removed_ssa_definition.u16);
            }
        };
    }
}

static struct semilattice_value * get_semilattice_propagation_from_data(
        struct scp * scp,
        struct ssa_data_node * current_node
) {
    switch (current_node->type) {
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) current_node;
            struct semilattice_value * right = get_semilattice_propagation_from_data(scp, binary->right);
            struct semilattice_value * left = get_semilattice_propagation_from_data(scp, binary->left);
            return join_semilattice(scp, left, right, binary->operand);
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) current_node;
            return get_semilattice_propagation_from_data(scp, unary->operand);
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current_node;
            return get_semillatice_by_ssa_name(scp, get_ssa_name->ssa_name);
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * constant_node = (struct ssa_data_constant_node *) current_node;
            return alloc_single_const_value_semilattice(scp, constant_node->constant_lox_value);
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            return get_semilattice_phi(scp, (struct ssa_data_phi_node *) current_node);
        }
        //Multiple values:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CALL: {
            return alloc_bottom_semilatttice(scp);
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        default:
            runtime_panic("Unhandled ssa data control_node type %i in get_semilattice_initialization_from_data() in scp.c", current_node->type);
    }
}

struct constant_rewrite * rewrite_constant_expressions_propagation_data_node(
        struct scp * scp,
        struct ssa_data_node * current_node
) {
    switch (current_node->type) {
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call_node = (struct ssa_data_function_call_node *) current_node;
            for(int i = 0; i < call_node->n_arguments; i++){
                struct ssa_data_node * current_argument = call_node->arguments[i];
                call_node->arguments[i] = rewrite_constant_expressions_propagation_data_node(scp, current_argument)->node;
            }
            return alloc_constant_rewrite(scp, current_node, alloc_bottom_semilatttice(scp));
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current_node;
            struct constant_rewrite * right = rewrite_constant_expressions_propagation_data_node(scp, binary_node->right);
            struct constant_rewrite * left = rewrite_constant_expressions_propagation_data_node(scp, binary_node->left);
            binary_node->right = right->node;
            binary_node->left = left->node;

            struct semilattice_value * result = join_semilattice(scp, left->semilattice, right->semilattice, binary_node->operand);

            switch (result->type) {
                case SEMILATTICE_CONSTANT:
                    return create_constant_rewrite_from_result(scp, current_node, result->values);
                case SEMILATTICE_BOTTOM:
                    return alloc_constant_rewrite(scp, current_node, alloc_bottom_semilatttice(scp));
                case SEMILATTICE_TOP:
                    return alloc_constant_rewrite(scp, current_node, alloc_top_semilatttice(scp));
            }
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) current_node;
            struct constant_rewrite * unary_operand_node = rewrite_constant_expressions_propagation_data_node(scp, unary_node->operand);
            unary_node->operand = unary_operand_node->node;

            switch (unary_operand_node->semilattice->type) {
                case SEMILATTICE_CONSTANT: {
                    struct u64_set new_possible_values;
                    init_u64_set(&new_possible_values, GET_SCP_ALLOCATOR(scp));
                    FOR_EACH_U64_SET_VALUE(unary_operand_node->semilattice->values, current_operand) {
                        lox_value_t current_result = calculate_unary_lox(current_operand, unary_node->operator_type);
                        add_u64_set(&new_possible_values, current_result);
                    }
                    return create_constant_rewrite_from_result(scp, current_node, new_possible_values);
                }
                case SEMILATTICE_BOTTOM:
                    return alloc_constant_rewrite(scp, current_node, alloc_bottom_semilatttice(scp));
                case SEMILATTICE_TOP:
                    return alloc_constant_rewrite(scp, current_node, alloc_top_semilatttice(scp));
            }

            break;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current_node;
            struct semilattice_value * semilattice_ssa_name = get_semillatice_by_ssa_name(scp, get_ssa_name->ssa_name);

            if (semilattice_ssa_name->type == SEMILATTICE_CONSTANT && size_u64_set(semilattice_ssa_name->values) == 1) {
                lox_value_t ssa_value = get_first_value_u64_set(semilattice_ssa_name->values);
                struct ssa_data_constant_node * constant_node = create_ssa_const_node(ssa_value, NULL, GET_SSA_NODES_ALLOCATOR(scp));
                return alloc_constant_rewrite(scp, &constant_node->data, alloc_single_const_value_semilattice(scp, ssa_value));
            } else {
                return alloc_constant_rewrite(scp, current_node, semilattice_ssa_name);
            }
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * const_node = (struct ssa_data_constant_node *) current_node;
            return alloc_constant_rewrite(scp, current_node, alloc_single_const_value_semilattice(scp, const_node->constant_lox_value));
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            return alloc_constant_rewrite(scp, current_node, alloc_bottom_semilatttice(scp));
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_data_phi_node * phi_node = (struct ssa_data_phi_node *) current_node;
            struct semilattice_value * semilattice_phi = get_semilattice_phi(scp, phi_node);
            return alloc_constant_rewrite(scp, current_node, semilattice_phi);
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            runtime_panic("Illegal get local control_node in sparse constant scp ssa optimization");
        }
    }

    return NULL;
}

struct constant_rewrite * rewrite_constant_expressions_initialization_data_node(
        struct scp * scp,
        struct ssa_data_node * current_node
) {
    switch (current_node->type) {
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call_node = (struct ssa_data_function_call_node *) current_node;
            for(int i = 0; i < call_node->n_arguments; i++){
                struct ssa_data_node * current_argument = call_node->arguments[i];
                call_node->arguments[i] = rewrite_constant_expressions_initialization_data_node(scp, current_argument)->node;
            }

            return alloc_constant_rewrite(scp, current_node, alloc_bottom_semilatttice(scp));
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current_node;
            struct constant_rewrite * right = rewrite_constant_expressions_initialization_data_node(scp, binary_node->right);
            struct constant_rewrite * left = rewrite_constant_expressions_initialization_data_node(scp, binary_node->left);
            binary_node->right = right->node;
            binary_node->left = left->node;

            if(left->semilattice->type == SEMILATTICE_CONSTANT && right->semilattice->type == SEMILATTICE_CONSTANT) {
                struct semilattice_value * result = join_semilattice(scp, left->semilattice, right->semilattice, binary_node->operand);
                return create_constant_rewrite_from_result(scp, current_node, result->values);
            } else {
                struct semilattice_value * result = join_semilattice(scp, left->semilattice, right->semilattice, binary_node->operand);
                return alloc_constant_rewrite(scp, current_node, result);
            }

            runtime_panic("Illegal code path in rewrite_constant_expressions_initialization_data_node");
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) current_node;
            struct constant_rewrite * unary_operand_node = rewrite_constant_expressions_initialization_data_node(scp, unary_node->operand);
            unary_node->operand = unary_operand_node->node;

            switch (unary_operand_node->semilattice->type) {
                case SEMILATTICE_CONSTANT: {
                    lox_value_t unary_operand = get_first_value_u64_set(unary_operand_node->semilattice->values);
                    lox_value_t unary_result = calculate_unary_lox(unary_operand, unary_node->operator_type);
                    return alloc_constant_rewrite(scp, current_node, alloc_single_const_value_semilattice(scp, unary_result));
                }
                case SEMILATTICE_BOTTOM: {
                    return alloc_constant_rewrite(scp, current_node, alloc_bottom_semilatttice(scp));
                }
                case SEMILATTICE_TOP: {
                    return alloc_constant_rewrite(scp, current_node, alloc_top_semilatttice(scp));
                }
            }
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME:
        case SSA_DATA_NODE_TYPE_PHI: {
            return alloc_constant_rewrite(scp, current_node, alloc_top_semilatttice(scp));
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * const_node = (struct ssa_data_constant_node *) current_node;
            return alloc_constant_rewrite(scp, current_node, alloc_single_const_value_semilattice(scp, const_node->constant_lox_value));
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            return alloc_constant_rewrite(scp, current_node, alloc_bottom_semilatttice(scp));
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            runtime_panic("Illegal get local control_node in sparse constant scp ssa optimization");
        }
    }

    return NULL;
}

static struct semilattice_value * get_semilattice_initialization_from_data(struct scp * scp, struct ssa_data_node *current_data_node) {
    switch (current_data_node->type) {
        case SSA_DATA_NODE_TYPE_BINARY: {
            return alloc_top_semilatttice(scp);
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            return alloc_top_semilatttice(scp);
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current_data_node;
            if (get_ssa_name->ssa_name.value.version == 0) {
                return alloc_bottom_semilatttice(scp); //Function parameter, can have multiple values
            } else {
                return alloc_top_semilatttice(scp);
            }
        }
        //Constant value:
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * constant_node = (struct ssa_data_constant_node *) current_data_node;
            return alloc_single_const_value_semilattice(scp, constant_node->constant_lox_value);
        }
        //Unknown value
        case SSA_DATA_NODE_TYPE_PHI: {
            return alloc_top_semilatttice(scp);
        }
        //Multiple values:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CALL: {
            return alloc_bottom_semilatttice(scp);
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        default:
            runtime_panic("Unhandled ssa data control_node type %i in get_semilattice_initialization_from_data() in scp.c", current_data_node->type);
    }
}

static void rewrite_constant_expressions_propagation_consumer(
        struct ssa_data_node * _,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * extra
) {
    struct scp * scp = extra;
    *parent_child_ptr = rewrite_constant_expressions_propagation_data_node(scp, current_node)->node;
}

static void rewrite_constant_expressions_propagation(
        struct scp * scp,
        struct ssa_control_node * current_node
) {
    // We iterate all the ssa_data_nodes in method rewrite_constant_expressions_propagation_consumer
    for_each_data_node_in_control_node(
            current_node,
            scp,
            SSA_DATA_NODE_OPT_POST_ORDER | SSA_DATA_NODE_OPT_RECURSIVE,
            rewrite_constant_expressions_propagation_consumer
    );
}

static void rewrite_constant_expressions_initialization_consumer(
        struct ssa_data_node * _,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * extra
) {
    struct scp * scp = extra;
    *parent_child_ptr = rewrite_constant_expressions_initialization_data_node(scp, current_node)->node;
}

static void rewrite_constant_expressions_initialization(
        struct scp * scp,
        struct ssa_control_node * current_node
) {
    for_each_data_node_in_control_node(
            current_node,
            scp,
            SSA_DATA_NODE_OPT_POST_ORDER | SSA_DATA_NODE_OPT_RECURSIVE,
            rewrite_constant_expressions_initialization_consumer
    );
}

static lox_value_t calculate_unary_lox(lox_value_t operand_value, ssa_unary_operator_type_t operator) {
    switch (operator) {
        case UNARY_OPERATION_TYPE_NEGATION: {
            return TO_LOX_VALUE_NUMBER(-AS_NUMBER(operand_value));
        }
        case UNARY_OPERATION_TYPE_NOT: {
            return TO_LOX_VALUE_BOOL(!AS_BOOL(operand_value));
        }
        default:
            runtime_panic("Unhandled unary operator type %i in calculate_unary() in scp.c", operator);
    }
}

struct scp * alloc_sparse_constant_propagation(struct ssa_ir * ssa_ir) {
    struct scp * scp = NATIVE_LOX_MALLOC(sizeof(struct scp));
    scp->ssa_ir = ssa_ir;
    struct arena arena;
    init_arena(&arena);
    scp->scp_allocator = to_lox_allocator_arena(arena);

    init_u64_hash_table(&scp->semilattice_by_ssa_name, GET_SCP_ALLOCATOR(scp));
    init_u64_set(&scp->removed_ssa_names, GET_SCP_ALLOCATOR(scp));
    init_stack_list(&scp->pending, GET_SCP_ALLOCATOR(scp));

    return scp;
}

static lox_value_t calculate_binary_lox(lox_value_t left, lox_value_t right, bytecode_t operator) {
    switch (operator) {
        case OP_ADD: return addition_lox(left, right);
        case OP_SUB: return TO_LOX_VALUE_NUMBER(AS_NUMBER(left) - AS_NUMBER(right));
        case OP_MUL: return TO_LOX_VALUE_NUMBER(AS_NUMBER(left) * AS_NUMBER(right));
        case OP_DIV: return TO_LOX_VALUE_NUMBER(AS_NUMBER(left) / AS_NUMBER(right));
        case OP_GREATER: return TO_LOX_VALUE_BOOL(AS_NUMBER(left) > AS_NUMBER(right));
        case OP_LESS: return TO_LOX_VALUE_BOOL(AS_NUMBER(left) < AS_NUMBER(right));
        case OP_EQUAL: return TO_LOX_VALUE_BOOL(left == right);
        default: runtime_panic("Unhandled binary operator %i in calculate_binary_lox() in scp.c", operator);
    }
}

static void rewrite_graph_as_constant(
        struct scp * scp,
        struct ssa_data_node * old_node,
        struct ssa_data_node ** parent_ptr,
        lox_value_t constant
) {
    struct ssa_data_constant_node * constant_node = create_ssa_const_node(constant, NULL, GET_SSA_NODES_ALLOCATOR(scp));
    *parent_ptr = &constant_node->data;
    free_ssa_data_node(old_node);
}

static struct u64_set_iterator node_uses_by_ssa_name_iterator(
        struct scp * scp,
        struct u64_hash_table node_uses_by_ssa_name,
        struct ssa_name ssa_name
) {
    if(contains_u64_hash_table(&node_uses_by_ssa_name, ssa_name.u16)){
        struct u64_set * node_uses_by_ssa_name_set = get_u64_hash_table(&node_uses_by_ssa_name, ssa_name.u16);
        struct u64_set_iterator node_uses_by_ssa_name_iterator;
        init_u64_set_iterator(&node_uses_by_ssa_name_iterator, *node_uses_by_ssa_name_set);
        return node_uses_by_ssa_name_iterator;
    } else {
        //Return emtpy iterator
        struct u64_set empty_set;
        init_u64_set(&empty_set, GET_SCP_ALLOCATOR(scp));
        struct u64_set_iterator node_uses_by_ssa_name_empty_iterator;
        init_u64_set_iterator(&node_uses_by_ssa_name_empty_iterator, empty_set);
        return node_uses_by_ssa_name_empty_iterator;
    }
}

static struct semilattice_value * alloc_multiple_const_values_semilattice(struct scp * scp, struct u64_set values) {
    struct semilattice_value * semilattice = LOX_MALLOC(GET_SCP_ALLOCATOR(scp), sizeof(struct semilattice_value));
    init_u64_set(&semilattice->values, GET_SCP_ALLOCATOR(scp));
    semilattice->type = SEMILATTICE_CONSTANT;
    union_u64_set(&semilattice->values, values);
    return semilattice;
}

static struct semilattice_value * alloc_single_const_value_semilattice(struct scp * scp, lox_value_t value) {
    struct semilattice_value * semilattice = LOX_MALLOC(GET_SCP_ALLOCATOR(scp), sizeof(struct semilattice_value));
    init_u64_set(&semilattice->values, GET_SCP_ALLOCATOR(scp));
    semilattice->type = SEMILATTICE_CONSTANT;
    add_u64_set(&semilattice->values, value);
    return semilattice;
}

static struct semilattice_value * alloc_top_semilatttice(struct scp * scp) {
    struct semilattice_value * semilattice = LOX_MALLOC(GET_SCP_ALLOCATOR(scp), sizeof(struct semilattice_value));
    init_u64_set(&semilattice->values, GET_SCP_ALLOCATOR(scp));
    semilattice->type = SEMILATTICE_TOP;
    return semilattice;
}

static struct semilattice_value * alloc_bottom_semilatttice(struct scp * scp) {
    struct semilattice_value * semilattice = LOX_MALLOC(GET_SCP_ALLOCATOR(scp), sizeof(struct semilattice_value));
    init_u64_set(&semilattice->values, GET_SCP_ALLOCATOR(scp));
    semilattice->type = SEMILATTICE_BOTTOM;
    return semilattice;
}

static void free_sparse_constant_propagation(struct scp * scp) {
    free_stack_list(&scp->pending);
    struct u64_hash_table_iterator iterator;
    init_u64_hash_table_iterator(&iterator, scp->semilattice_by_ssa_name);
    NATIVE_LOX_FREE(scp);
}

static struct semilattice_value * alloc_semilatttice(struct scp * scp, semilattice_type_t type, struct u64_set values) {
    struct semilattice_value * semilattice = LOX_MALLOC(GET_SCP_ALLOCATOR(scp), sizeof(struct semilattice_value));
    semilattice->values = values;
    semilattice->type = type;
    return semilattice;
}

static struct semilattice_value * get_semilattice_phi(
        struct scp * scp,
        struct ssa_data_phi_node * phi_node
) {
    semilattice_type_t final_semilattice_type = SEMILATTICE_TOP;
    bool top_value_found = false;
    struct u64_set final_values;
    init_u64_set(&final_values, GET_SCP_ALLOCATOR(scp));

    FOR_EACH_VERSION_IN_PHI_NODE(phi_node, current_name) {
        struct semilattice_value * current_semilatice = get_semillatice_by_ssa_name(scp, current_name);

        union_u64_set(&final_values, current_semilatice->values);

        if(final_semilattice_type == SEMILATTICE_BOTTOM) {
            continue;
        } else if(current_semilatice->type == SEMILATTICE_BOTTOM) {
            final_semilattice_type = SEMILATTICE_BOTTOM;
        } else if(current_semilatice->type == SEMILATTICE_TOP){
            final_semilattice_type = SEMILATTICE_TOP;
            top_value_found = true;
        } else if(current_semilatice->type == SEMILATTICE_CONSTANT && !top_value_found && size_u64_set(final_values) == 1){
            final_semilattice_type = SEMILATTICE_CONSTANT;
        } else if(current_semilatice->type == SEMILATTICE_CONSTANT && !top_value_found && size_u64_set(final_values) > 1){
            final_semilattice_type = SEMILATTICE_TOP;
        }
    }

    return alloc_semilatttice(scp, final_semilattice_type, final_values);
}

static struct semilattice_value * join_semilattice(
        struct scp * scp,
        struct semilattice_value * left,
        struct semilattice_value * right,
        bytecode_t operator
) {
    if (left->type == SEMILATTICE_BOTTOM || right->type == SEMILATTICE_BOTTOM) {
        return alloc_top_semilatttice(scp);
    }

    struct u64_set final_values;
    init_u64_set(&final_values, GET_SCP_ALLOCATOR(scp));

    if(left->type == SEMILATTICE_TOP || right->type == SEMILATTICE_TOP){
        union_u64_set(&final_values, right->values);
        union_u64_set(&final_values, left->values);
        return alloc_semilatttice(scp, SEMILATTICE_TOP, final_values);
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

    return alloc_semilatttice(scp, SEMILATTICE_CONSTANT, final_values);
}

static struct constant_rewrite * create_constant_rewrite_from_result(
        struct scp * scp,
        struct ssa_data_node * data_node,
        struct u64_set possible_values
) {
    if(size_u64_set(possible_values) == 1){
        lox_value_t value = get_first_value_u64_set(possible_values);
        struct ssa_data_node * const_node = (struct ssa_data_node *) create_ssa_const_node(value, NULL, GET_SCP_ALLOCATOR(scp));
        return alloc_constant_rewrite(scp, const_node, alloc_single_const_value_semilattice(scp, value));
    } else {
        return alloc_constant_rewrite(scp, data_node, alloc_multiple_const_values_semilattice(scp, possible_values));
    }
}

static struct constant_rewrite * alloc_constant_rewrite(struct scp * scp, struct ssa_data_node * node, struct semilattice_value * value) {
    struct constant_rewrite * constant_rewrite = LOX_MALLOC(GET_SCP_ALLOCATOR(scp), sizeof(struct constant_rewrite));
    constant_rewrite->semilattice = value;
    constant_rewrite->node = node;
    return constant_rewrite;
}

static void remove_from_ir_removed_ssa_names(struct scp * scp) {
    remove_names_references_ssa_ir(scp->ssa_ir, scp->removed_ssa_names);
}

static struct semilattice_value * get_semillatice_by_ssa_name(struct scp * scp, struct ssa_name ssa_name) {
    struct semilattice_value * semilattice_value = get_u64_hash_table(&scp->semilattice_by_ssa_name, ssa_name.u16);
    //If it is null, it means that the ssa name comes from a function argument, which is used but not defined in the ir
    //A function argument is always bottom, since it could contain multiple values
    if (semilattice_value == NULL){
        struct semilattice_value * semilattice_value_func_arg = alloc_bottom_semilatttice(scp);
        put_u64_hash_table(&scp->semilattice_by_ssa_name, ssa_name.u16, semilattice_value_func_arg);
        return semilattice_value_func_arg;
    } else {
        return semilattice_value;
    }
}