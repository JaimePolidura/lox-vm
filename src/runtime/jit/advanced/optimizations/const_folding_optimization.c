#include "const_folding_optimization.h"

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

struct const_folding_optimizer {
    struct stack_list pending;
    struct u64_hash_table ssa_definitions_by_ssa_name;
    struct u64_hash_table semilattice_type_by_ssa_name;
};

static void initialization(struct const_folding_optimizer * optimizer);
static void propagation(struct const_folding_optimizer * optimizer);

static struct semilattice_value create_semilattice_from_data(struct ssa_data_node *, struct ssa_data_node **);
struct const_folding_optimizer * alloc_struct_const_folding_optimizer(struct phi_insertion_result);
static struct semilattice_value calculate_unary(struct semilattice_value, ssa_unary_operator_type_t operator);
static struct semilattice_value calculate_binary(struct semilattice_value, struct semilattice_value, bytecode_t operator);
extern lox_value_t addition_lox(lox_value_t a, lox_value_t b);
static lox_value_type calculate_binary_lox(lox_value_t, lox_value_t, bytecode_t operator);
static void rewrite_graph_as_constant(struct ssa_data_node * old_node, struct ssa_data_node ** parent_ptr, lox_value_t constant);

void perform_const_folding_optimization(struct ssa_block * start_block, struct phi_insertion_result phi_insertion_result) {
    struct const_folding_optimizer * optimizer = alloc_struct_const_folding_optimizer(phi_insertion_result);
    start_block = start_block->next.next;

    initialization(optimizer);
    propagation(optimizer);
}

static void propagation(struct const_folding_optimizer * optimizer) {
    while(!is_empty_stack_list(&optimizer->pending)) {
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64((uint64_t) pop_stack_list(&optimizer->pending));
    }
}

//Initializes list of ssa names to work with
//Also rewrites constant expressions like: 1 + 1 -> 2
static void initialization(struct const_folding_optimizer * optimizer) {
    struct u64_hash_table_iterator ssa_names_iterator;
    init_u64_hash_table_iterator(&ssa_names_iterator, optimizer->ssa_definitions_by_ssa_name);

    while(has_next_u64_hash_table_iterator(ssa_names_iterator)){
        struct u64_hash_table_entry entry = next_u64_hash_table_iterator(&ssa_names_iterator);
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64(entry.key);
        struct ssa_control_define_ssa_name_node * current_definition = entry.value;

        struct semilattice_value semilattice_value = create_semilattice_from_data(current_definition->value, &current_definition->value);

        put_u64_hash_table(&optimizer->semilattice_type_by_ssa_name, current_ssa_name.u16, (void*) semilattice_value.type);

        if (semilattice_value.type != SEMILATTICE_TOP) {
            push_stack_list(&optimizer->pending, (void *) current_ssa_name.u16);
        }
    }
}

static struct semilattice_value create_semilattice_from_data(
        struct ssa_data_node * current_data_node,
        struct ssa_data_node ** parent_ptr
) {
    switch (current_data_node->type) {
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current_data_node;
            struct semilattice_value right_semilattice = create_semilattice_from_data(binary_node->right, &binary_node->right);
            struct semilattice_value left_semilattice = create_semilattice_from_data(binary_node->left, &binary_node->left);
            struct semilattice_value result_semilattice = calculate_binary(left_semilattice, right_semilattice, binary_node->operand);

            if (result_semilattice.type == SEMILATTICE_CONSTANT) {
                rewrite_graph_as_constant(&binary_node->data, parent_ptr, result_semilattice.const_value);
            }

            return result_semilattice;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) current_data_node;
            struct semilattice_value operand_semilattice = create_semilattice_from_data(unary_node->operand, &unary_node->operand);
            struct semilattice_value result_semilattice = calculate_unary(operand_semilattice, unary_node->operator_type);

            if (result_semilattice.type == SEMILATTICE_CONSTANT) {
                rewrite_graph_as_constant(&unary_node->data, parent_ptr, result_semilattice.const_value);
            }

            return result_semilattice;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current_data_node;
            struct ssa_control_define_ssa_name_node * definition_node = get_ssa_name->definition_node;
            return create_semilattice_from_data(definition_node->value, &definition_node->value);
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
            //Not possible
            exit(-1);
    }
}

static struct semilattice_value calculate_binary(
        struct semilattice_value left,
        struct semilattice_value right,
        bytecode_t operator
) {
    if (left.type == right.type) {
        if(left.type != SEMILATTICE_CONSTANT) {
            return left;
        } else {
            lox_value_t right_value = right.const_value;
            lox_value_t left_value = left.const_value;
            lox_value_t result = calculate_binary_lox(left_value, right_value, operator);
            return CREATE_CONSTANT_SEMILATTICE(result);
        }
    }

    struct semilattice_value stronger = left.type >= right.type ? left : right;
    struct semilattice_value weaker = left.type >= right.type ? right : left;

    return stronger;
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
                    exit(-1);
            }
    }
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
        default: exit(-1);
    }
}

struct const_folding_optimizer * alloc_struc_const_folding_optimizer(struct phi_insertion_result phi_insertion_result) {
    struct const_folding_optimizer * to_return = malloc(sizeof(struct const_folding_optimizer));
    to_return->ssa_definitions_by_ssa_name = phi_insertion_result.ssa_definitions_by_ssa_name;
    init_u64_hash_table(&to_return->semilattice_type_by_ssa_name);
    init_stack_list(&to_return->pending);
    return to_return;
}

static void rewrite_graph_as_constant(struct ssa_data_node * old_node, struct ssa_data_node ** parent_ptr, lox_value_t constant) {
    struct ssa_data_constant_node * constant_node = create_ssa_const_node(constant, NULL);
    *parent_ptr = &constant_node->data;
    free_ssa_data_node(old_node);
}