#include "ir_lowerer_data.h"

typedef struct lox_ir_ll_operand(* lowerer_lox_ir_data_t)(struct lllil*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_unary(struct lllil*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_constant(struct lllil*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_v_register(struct lllil*, struct lox_ir_data_node*);

static struct lox_ir_ll_operand emit_not_lox(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_not_native(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_negate_lox(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_negate_native(struct lllil*, struct lox_ir_data_node*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand new_v_register(struct lllil *, bool is_float);

extern void runtime_panic(char * format, ...);

lowerer_lox_ir_data_t lowerer_lox_ir_by_data_node[] = {
        [LOX_IR_DATA_NODE_GET_V_REGISTER] = lowerer_lox_ir_data_get_v_register,
        [LOX_IR_DATA_NODE_CONSTANT] = lowerer_lox_ir_data_constant,
        [LOX_IR_DATA_NODE_UNARY] = lowerer_lox_ir_data_unary,

        [LOX_IR_DATA_NODE_CALL] = NULL,
        [LOX_IR_DATA_NODE_GET_GLOBAL] = NULL,
        [LOX_IR_DATA_NODE_BINARY] = NULL,
        [LOX_IR_DATA_NODE_GET_STRUCT_FIELD] = NULL,
        [LOX_IR_DATA_NODE_INITIALIZE_STRUCT] = NULL,
        [LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT] = NULL,
        [LOX_IR_DATA_NODE_INITIALIZE_ARRAY] = NULL,
        [LOX_IR_DATA_NODE_GET_ARRAY_LENGTH] = NULL,
        [LOX_IR_DATA_NODE_GUARD] = NULL,
        [LOX_IR_DATA_NODE_UNBOX] = NULL,
        [LOX_IR_DATA_NODE_BOX] = NULL,

        [LOX_IR_DATA_NODE_GET_SSA_NAME] = NULL,
        [LOX_IR_DATA_NODE_GET_LOCAL] = NULL,
        [LOX_IR_DATA_NODE_PHI] = NULL,
};

//If expected type is:
// - LOX_IR_TYPE_LOX_ANY the result type will have lox byte format
// - LOX_IR_TYPE_UNKNOWN the result type will have any type
struct lox_ir_ll_operand lower_lox_ir_data(
        struct lllil * lllil,
        struct lox_ir_data_node * data_node,
        lox_ir_type_t expected_type
) {
    lowerer_lox_ir_data_t lowerer_lox_ir_data = lowerer_lox_ir_by_data_node[data_node->type];
    if(lowerer_lox_ir_data == NULL){
        runtime_panic("Unexpected data node %i in ir_lowerer_data", data_node->type);
    }

    struct lox_ir_ll_operand operand_result = lowerer_lox_ir_data(lllil, data_node);

    return operand_result;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_v_register(
        struct lllil * lllil,
        struct lox_ir_data_node * node
) {
    struct lox_ir_data_get_v_register_node * get_v_reg = (struct lox_ir_data_get_v_register_node *) node;
    return V_REG_TO_OPERAND(get_v_reg->v_register);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_constant(
        struct lllil * lllil,
        struct lox_ir_data_node * node
) {
    struct lox_ir_data_constant_node * const_node = (struct lox_ir_data_constant_node *) node;
    return IMMEDIATE_TO_OPERAND(const_node->value);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_unary(
        struct lllil * lllil,
        struct lox_ir_data_node * data
) {
    struct lox_ir_data_unary_node * unary = (struct lox_ir_data_unary_node *) data;
    struct lox_ir_data_node * unary_operand_node = unary->operand;
    struct lox_ir_ll_operand unary_operand = lower_lox_ir_data(lllil, unary_operand_node, LOX_IR_TYPE_UNKNOWN);
    bool unary_operand_is_lox = is_lox_lox_ir_type(unary->operand->produced_type->type);

    if (unary->operator == UNARY_OPERATION_TYPE_NOT && unary_operand_is_lox) {
        return emit_not_lox(lllil, unary_operand);
    } else if (unary->operator == UNARY_OPERATION_TYPE_NOT) {
        return emit_not_native(lllil, unary_operand);
    } else if(unary->operator == UNARY_OPERATION_TYPE_NEGATION && unary_operand_is_lox) {
        return emit_negate_lox(lllil, unary_operand);
    } else {
        return emit_negate_native(lllil, unary_operand_node, unary_operand);
    }

    return unary_operand;
}

//True value_node:  0x7ffc000000000003
//False value_node: 0x7ffc000000000002
//value1 = value0 + ((TRUE - value0) + (FALSE - value0))
//value1 = - value0 + TRUE + FALSE
//value1 = (TRUE - value_node) + FALSE
static struct lox_ir_ll_operand emit_not_lox(
        struct lllil * lllil,
        struct lox_ir_ll_operand unary_operand //Expect to be in register
) {
    struct lox_ir_ll_operand true_value_reg = new_v_register(lllil, false);
    struct lox_ir_ll_operand false_value_reg = new_v_register(lllil, false);

    emit_move_ll_lox_ir(lllil, true_value_reg, IMMEDIATE_TO_OPERAND(TRUE_VALUE));

    emit_isub_ll_lox_ir(lllil, true_value_reg, unary_operand);

    emit_move_ll_lox_ir(lllil, false_value_reg, IMMEDIATE_TO_OPERAND(FALSE_VALUE));

    emit_iadd_ll_lox_ir(lllil, unary_operand, false_value_reg);

    return unary_operand;
}

static struct lox_ir_ll_operand emit_not_native(
        struct lllil * lllil,
        struct lox_ir_ll_operand unary_operand //Expect to be in register
) {
    emit_unary_ll_lox_ir(lllil, unary_operand, UNARY_LL_LOX_IR_LOGICAL_NEGATION);
    return unary_operand;
}

static struct lox_ir_ll_operand emit_negate_lox(struct lllil * lllil, struct lox_ir_ll_operand unary_operand) {
    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_LOGICAL_XOR,
            unary_operand,
            IMMEDIATE_TO_OPERAND(0x8000000000000000)
    );
    return unary_operand;
}

static struct lox_ir_ll_operand emit_negate_native(
        struct lllil * lllil,
        struct lox_ir_data_node * unary_operand_node,
        struct lox_ir_ll_operand unary_operand
) {
    if (unary_operand_node->produced_type->type == LOX_IR_TYPE_F64) { //Float
        emit_binary_ll_lox_ir(
                lllil,
                BINARY_LL_LOX_IR_LOGICAL_XOR,
                unary_operand,
                IMMEDIATE_TO_OPERAND(0x8000000000000000)
        );
    } else {
        emit_unary_ll_lox_ir(lllil, unary_operand, UNARY_LL_LOX_IR_NUMBER_NEGATION);
    }

    return unary_operand;
}

static struct lox_ir_ll_operand new_v_register(struct lllil * lllil, bool is_float) {
    return V_REG_TO_OPERAND(((struct v_register) {
            .reg_number = lllil->lox_ir->last_v_reg_allocated++,
            .is_float_register = is_float,
    }));
}