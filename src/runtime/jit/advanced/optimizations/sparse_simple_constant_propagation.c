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
    lox_value_t const_value; //Only used when type is SEMILATTICE_CONSTANT
};

#define CREATE_TOP_SEMILATTICE() (struct semilattice_value) {.type = SEMILATTICE_TOP, .const_value = NIL_VALUE }
#define CREATE_CONSTANT_SEMILATTICE(constant) (struct semilattice_value) {.type = SEMILATTICE_CONSTANT, .const_value = constant }
#define CREATE_BOTTOM_SEMILATTICE() (struct semilattice_value) {.type = SEMILATTICE_BOTTOM, .const_value = NIL_VALUE }

struct sparse_simple_constant_propagation {
    struct stack_list pending;
    struct u64_hash_table propagtion;
    struct ssa_ir * ssa_ir;
};

extern lox_value_t addition_lox(lox_value_t a, lox_value_t b);
extern void runtime_panic(char * format, ...);

static void initialization(struct sparse_simple_constant_propagation * sscp);
static void propagation(struct sparse_simple_constant_propagation * sscp);

static struct semilattice_value get_semilattice_from_data(struct ssa_data_node *);
struct sparse_simple_constant_propagation * alloc_sparse_constant_propagation(struct ssa_ir *);
static void free_sparse_constant_propagation(struct sparse_simple_constant_propagation *);
static struct semilattice_value calculate_unary(struct semilattice_value, ssa_unary_operator_type_t operator);
static lox_value_type calculate_binary_lox(lox_value_t, lox_value_t, bytecode_t operator);
static lox_value_type calculate_unary_lox(lox_value_t, ssa_unary_operator_type_t operator);
static void rewrite_graph_as_constant(struct ssa_data_node * old_node, struct ssa_data_node ** parent_ptr, lox_value_t constant);
static struct u64_set_iterator node_uses_by_ssa_name_iterator(struct u64_hash_table, struct ssa_name);
static void rewrite_constant_expressions(struct sparse_simple_constant_propagation *, struct ssa_control_node *);
static struct semilattice_value * alloc_semilattice(struct semilattice_value);

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
                        &sscp->propagtion, define_ssa_name.u16);

                if (prev_semilattice->type != SEMILATTICE_BOTTOM) {
                    struct semilattice_value current_semilattice = get_semilattice_from_data(define_ssa_name_node->value);
                    put_u64_hash_table(&sscp->propagtion, current_ssa_name.u16, alloc_semilattice(current_semilattice));

                    if(current_semilattice.type != prev_semilattice->type){
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

        struct semilattice_value semilattice_value = get_semilattice_from_data(nullptr);
        put_u64_hash_table(&sscp->propagtion, current_ssa_name.u16, alloc_semilattice(semilattice_value));

        if (semilattice_value.type != SEMILATTICE_TOP) {
            push_stack_list(&sscp->pending, (void *) current_ssa_name.u16);
        }
    }
}

static struct semilattice_value get_semilattice_from_data(struct ssa_data_node *current_data_node) {
    switch (current_data_node->type) {
        case SSA_DATA_NODE_TYPE_BINARY: {
            return CREATE_TOP_SEMILATTICE();
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            return CREATE_TOP_SEMILATTICE();
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current_data_node;
            if (get_ssa_name->ssa_name.value.version == 0) {
                return CREATE_BOTTOM_SEMILATTICE(); //Function parameter, can have multiple values
            } else {
                return CREATE_TOP_SEMILATTICE();
            }
        }
        //Constant value:
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * constant_node = (struct ssa_data_constant_node *) current_data_node;
            return CREATE_CONSTANT_SEMILATTICE(constant_node->constant_lox_value);
        }
        //Unknown value
        case SSA_DATA_NODE_TYPE_PHI: {
            return CREATE_TOP_SEMILATTICE();
        }
        //Multiple values:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CALL: {
            return CREATE_BOTTOM_SEMILATTICE();
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        default:
            runtime_panic("Unhandled ssa data node type %i in get_semilattice_from_data() in sparse_simple_constant_propagation.c", current_data_node->type);
    }
}

struct ssa_data_node * rewrite_constant_expressions_data_node(
        struct sparse_simple_constant_propagation * sscp,
        struct ssa_data_node * current_node
) {
    switch (current_node->type) {
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call_node = (struct ssa_data_function_call_node *) current_node;
            for(int i = 0; i < call_node->n_arguments; i++){
                struct ssa_data_node * current_argument = call_node->arguments[i];
                call_node->arguments[i] = rewrite_constant_expressions_data_node(sscp, current_argument);
            }
            return current_node;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current_node;
            struct ssa_data_node * right = rewrite_constant_expressions_data_node(sscp, binary_node->right);
            struct ssa_data_node * left = rewrite_constant_expressions_data_node(sscp, binary_node->left);
            binary_node->right = right;
            binary_node->left = left;

            if(left->type == SSA_DATA_NODE_TYPE_CONSTANT && right->type == SSA_DATA_NODE_TYPE_CONSTANT) {
                lox_value_t right_constant = ((struct ssa_data_constant_node *) right)->constant_lox_value;
                lox_value_t left_constant = ((struct ssa_data_constant_node *) left)->constant_lox_value;
                lox_value_type binary_op_result = calculate_binary_lox(left_constant, right_constant, binary_node->operand);
                struct ssa_data_constant_node * new_constant_node = create_ssa_const_node(binary_op_result, NULL);
                return &new_constant_node->data;
            } else {
                return current_node;
            }
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) current_node;
            struct ssa_data_node * unary_operand_node = rewrite_constant_expressions_data_node(sscp, unary_node->operand);

            if(unary_operand_node->type == SSA_DATA_NODE_TYPE_CONSTANT){
                lox_value_t unary_operand_constant = ((struct ssa_data_constant_node *) unary_operand_node)->constant_lox_value;
                lox_value_t unary_op_result = calculate_unary_lox(unary_operand_constant, unary_node->operator_type);
                struct ssa_data_constant_node * new_constant_node = create_ssa_const_node(unary_op_result, NULL);
                return &new_constant_node->data;
            } else {
                return current_node;
            }

            break;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current_node;
            struct semilattice_value * semilattice_ssa_name = get_u64_hash_table(&sscp->propagtion, get_ssa_name->ssa_name.u16);
            if(semilattice_ssa_name->type == SEMILATTICE_CONSTANT){
                struct ssa_data_constant_node * constant_node = create_ssa_const_node(semilattice_ssa_name->const_value, NULL);
                return &constant_node->data;
            } else {
                return current_node;
            }
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CONSTANT:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
        case SSA_DATA_NODE_TYPE_PHI: {
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

static struct semilattice_value calculate_unary(struct semilattice_value operand_value, ssa_unary_operator_type_t operator) {
    switch (operand_value.type) {
        case SEMILATTICE_BOTTOM:
        case SEMILATTICE_TOP:
            return operand_value;
        case SEMILATTICE_CONSTANT:
            switch (operator) {
                case UNARY_OPERATION_TYPE_NEGATION: {
                    lox_value_t negated_value = TO_LOX_VALUE_NUMBER(-AS_NUMBER(operand_value.const_value));
                    return CREATE_CONSTANT_SEMILATTICE(negated_value);
                }
                case UNARY_OPERATION_TYPE_NOT: {
                    lox_value_t not_value = TO_LOX_VALUE_BOOL(!AS_BOOL(operand_value.const_value));
                    return CREATE_CONSTANT_SEMILATTICE(not_value);
                }
                default:
                    runtime_panic("Unhandled unary operator type %i in calculate_unary() in sparse_simple_constant_propagation.c", operand_value.type);
            }
    }
}

struct sparse_simple_constant_propagation * alloc_sparse_constant_propagation(struct ssa_ir * ssa_ir) {
    struct sparse_simple_constant_propagation * sscp = malloc(sizeof(struct sparse_simple_constant_propagation));
    init_u64_hash_table(&sscp->propagtion);
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

static struct semilattice_value * alloc_semilattice(struct semilattice_value semilattice_value) {
    struct semilattice_value * semilattice = malloc(sizeof(struct semilattice_value));
    semilattice->const_value = semilattice_value.const_value;
    semilattice->type = semilattice_value.type;
    return semilattice;
}

static void free_sparse_constant_propagation(struct sparse_simple_constant_propagation * sscp) {
    free_stack_list(&sscp->pending);
    struct u64_hash_table_iterator iterator;
    init_u64_hash_table_iterator(&iterator, sscp->propagtion);
    while(has_next_u64_hash_table_iterator(iterator)){
        struct semilattice_value * semilattice = next_u64_hash_table_iterator(&iterator).value;
        free(semilattice);
    }
    free(sscp);
}