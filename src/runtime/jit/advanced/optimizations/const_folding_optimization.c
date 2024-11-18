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

static struct semilattice_value * alloc_semilattice_value(semilattice_type_t type, lox_value_t value);

#define CREATE_TOP_SEMILATTICE() alloc_semilattice_value(SEMILATTICE_TOP, NIL_VALUE)
#define CREATE_CONSTANT_SEMILATTICE(constant) alloc_semilattice_value(SEMILATTICE_CONSTANT, constant)
#define CREATE_BOTTOM_SEMILATTICE() alloc_semilattice_value(SEMILATTICE_BOTTOM, NIL_VALUE)
static void free_semilattice_value(struct semilattice_value *);

struct const_folding_optimizer {
    struct stack_list pending;
    struct u64_hash_table ssa_definitions_by_ssa_name;
    struct u64_hash_table semilattice_values_by_ssa_name;
};

static struct semilattice_value * create_semilattice_from_data(struct ssa_data_node *);
struct const_folding_optimizer * alloc_struct_const_folding_optimizer(struct phi_insertion_result);
static void initialization(struct const_folding_optimizer * optimizer);
static struct semilattice_value * calculate_unary(struct semilattice_value *, ssa_unary_operator_type_t operator);
static struct semilattice_value * calculate_binary(struct semilattice_value *, struct semilattice_value *, bytecode_t operator);
extern lox_value_t addition_lox(lox_value_t a, lox_value_t b);
static lox_value_type calculate_binary_lox(lox_value_t, lox_value_t, bytecode_t operator);

void perform_const_folding_optimization(struct ssa_block * start_block, struct phi_insertion_result phi_insertion_result) {
    struct const_folding_optimizer * optimizer = alloc_struct_const_folding_optimizer(phi_insertion_result);
    start_block = start_block->next.next;

    initialization(optimizer);
}

static void initialization(struct const_folding_optimizer * optimizer) {
    struct u64_hash_table_iterator ssa_names_iterator;
    init_u64_hash_table_iterator(&ssa_names_iterator, optimizer->ssa_definitions_by_ssa_name);

    while(has_next_u64_hash_table_iterator(ssa_names_iterator)){
        struct u64_hash_table_entry entry = next_u64_hash_table_iterator(&ssa_names_iterator);
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64(entry.key);
        struct ssa_control_define_ssa_name_node * current_ssa_definition = entry.value;

        struct semilattice_value * semilattice_value = create_semilattice_from_data(current_ssa_definition->value);

        put_u64_hash_table(&optimizer->semilattice_values_by_ssa_name, current_ssa_name.u16, semilattice_value);

        if (semilattice_value->type != SEMILATTICE_TOP) {
            push_stack_list(&optimizer->pending, semilattice_value);
        } else {
            free_semilattice_value(semilattice_value);
        }
    }
}

static struct semilattice_value * create_semilattice_from_data(struct ssa_data_node * current_data_node) {
    switch (current_data_node->type) {
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current_data_node;
            struct semilattice_value * right_semilattice = create_semilattice_from_data(binary_node->right);
            struct semilattice_value * left_semilattice = create_semilattice_from_data(binary_node->left);
            return calculate_binary(left_semilattice, right_semilattice, binary_node->operand);
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) current_data_node;
            struct semilattice_value * operand_semilattice = create_semilattice_from_data(unary_node->operand);
            return calculate_unary(operand_semilattice, unary_node->operator_type);
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            return create_semilattice_from_data(current_data_node);
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

static struct semilattice_value * calculate_binary(
        struct semilattice_value * left,
        struct semilattice_value * right,
        bytecode_t operator
) {
    if (left->type == right->type) {
        if(left->type != SEMILATTICE_CONSTANT) {
            free_semilattice_value(right);
            return left;
        } else {
            lox_value_t right_value = right->const_value;
            lox_value_t left_value = left->const_value;
            lox_value_t result = calculate_binary_lox(left_value, right_value, operator);
            free_semilattice_value(right);
            free_semilattice_value(left);
            return CREATE_CONSTANT_SEMILATTICE(result);
        }
    }

    struct semilattice_value * stronger = left->type >= right->type ? left : right;
    struct semilattice_value * weaker = left->type >= right->type ? right : left;
    free_semilattice_value(weaker);
    return stronger;
}

static struct semilattice_value * calculate_unary(struct semilattice_value * operand_value, ssa_unary_operator_type_t operator) {
    switch (operand_value->type) {
        case SEMILATTICE_BOTTOM:
        case SEMILATTICE_TOP:
            return operand_value;
        case SEMILATTICE_CONSTANT:
            switch (operator) {
                case UNARY_OPERATION_TYPE_NEGATION: {
                    lox_value_t negated_value = TO_LOX_VALUE_NUMBER(-AS_NUMBER(operand_value->const_value));
                    free_semilattice_value(operand_value);
                    return CREATE_CONSTANT_SEMILATTICE(negated_value);
                }
                case UNARY_OPERATION_TYPE_NOT: {
                    lox_value_t not_value = TO_LOX_VALUE_BOOL(!AS_BOOL(operand_value->const_value));
                    free_semilattice_value(operand_value);
                    return CREATE_CONSTANT_SEMILATTICE(not_value);
                }
                default:
                    return NULL;
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
    init_u64_hash_table(&to_return->semilattice_values_by_ssa_name);
    init_stack_list(&to_return->pending);
    return to_return;
}

static struct semilattice_value * alloc_semilattice_value(semilattice_type_t type, lox_value_t value) {
    struct semilattice_value * semilattice_value = malloc(sizeof(struct semilattice_value));
    semilattice_value->const_value = value;
    semilattice_value->type = type;
    return semilattice_value;
}

static void free_semilattice_value(struct semilattice_value * semilattice_value) {
    //TODO
}