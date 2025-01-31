#include "ir_lowerer_data.h"

typedef struct lox_ir_ll_operand(* lowerer_lox_ir_data_t)(struct lllil*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_v_register(struct lllil*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_global(struct lllil*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_constant(struct lllil*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_unary(struct lllil*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_unbox(struct lllil*, struct lox_ir_data_node*);

static struct lox_ir_ll_operand emit_not_lox(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_not_native(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_negate_lox(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_negate_native(struct lllil*, struct lox_ir_data_node*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand alloc_new_v_register(struct lllil *lllil, bool is_float);
static struct lox_ir_ll_operand emit_lox_any_to_native_cast(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_number_to_native_i64_cast(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_boolean_to_native(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_string_to_native(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_array_to_native(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_struct_instance_to_native(struct lllil*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_object_ptr_to_native(struct lllil *lllil, struct lox_ir_ll_operand input);

extern uint64_t any_to_native_cast_jit_runime(lox_value_t);
extern void runtime_panic(char * format, ...);

lowerer_lox_ir_data_t lowerer_lox_ir_by_data_node[] = {
        [LOX_IR_DATA_NODE_GET_V_REGISTER] = lowerer_lox_ir_data_get_v_register,
        [LOX_IR_DATA_NODE_GET_GLOBAL] = lowerer_lox_ir_data_get_global,
        [LOX_IR_DATA_NODE_CONSTANT] = lowerer_lox_ir_data_constant,
        [LOX_IR_DATA_NODE_UNARY] = lowerer_lox_ir_data_unary,
        [LOX_IR_DATA_NODE_UNBOX] = lowerer_lox_ir_data_unbox,

        [LOX_IR_DATA_NODE_CALL] = NULL,
        [LOX_IR_DATA_NODE_BINARY] = NULL,
        [LOX_IR_DATA_NODE_GET_STRUCT_FIELD] = NULL,
        [LOX_IR_DATA_NODE_INITIALIZE_STRUCT] = NULL,
        [LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT] = NULL,
        [LOX_IR_DATA_NODE_INITIALIZE_ARRAY] = NULL,
        [LOX_IR_DATA_NODE_GET_ARRAY_LENGTH] = NULL,
        [LOX_IR_DATA_NODE_GUARD] = NULL,
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

static struct lox_ir_ll_operand lowerer_lox_ir_data_unbox(
        struct lllil * lllil,
        struct lox_ir_data_node * data_node
) {
    //TODO Make the guarantee to be in a vregister not an immediate value,
    struct lox_ir_ll_operand to_unbox_input = lower_lox_ir_data(lllil, data_node, LOX_IR_TYPE_UNKNOWN);
    struct lox_ir_data_unbox_node * unbox = (struct lox_ir_data_unbox_node *) data_node;

    switch (data_node->produced_type->type) {
        case LOX_IR_TYPE_LOX_NIL: {
            struct lox_ir_ll_operand nil_value_reg = V_REG_TO_OPERAND(alloc_v_register_lox_ir(lllil->lox_ir, false));
            emit_move_ll_lox_ir(lllil, nil_value_reg, IMMEDIATE_TO_OPERAND((uint64_t) NULL));
            return nil_value_reg;
        }
        case LOX_IR_TYPE_LOX_ANY: {
            return emit_lox_any_to_native_cast(lllil, to_unbox_input);
        }
        case LOX_IR_TYPE_LOX_I64: {
            return emit_lox_number_to_native_i64_cast(lllil, to_unbox_input);
        }
        case LOX_IR_TYPE_LOX_BOOLEAN: {
            return emit_lox_boolean_to_native(lllil, to_unbox_input);
        }
        case LOX_IR_TYPE_LOX_STRING: {
            return emit_lox_string_to_native(lllil, to_unbox_input);
        }
        case LOX_IR_TYPE_LOX_ARRAY: {
            return emit_lox_array_to_native(lllil, to_unbox_input);
        }
        case LOX_IR_TYPE_LOX_STRUCT_INSTANCE: {
            return emit_lox_struct_instance_to_native(lllil, to_unbox_input);
        }
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_global(
        struct lllil * lllil,
        struct lox_ir_data_node * data_node
) {
    struct lox_ir_data_get_global_node * get_global = (struct lox_ir_data_get_global_node *) data_node;
    struct v_register global_v_register = alloc_v_register_lox_ir(lllil->lox_ir, false);

    emit_function_call_with_return_value_ll_lox_ir(
            lllil,
            &get_u64_hash_table,
            global_v_register,
            2,
            IMMEDIATE_TO_OPERAND((uint64_t) &get_global->package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) &get_global->name)
    );

    return V_REG_TO_OPERAND(global_v_register);
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
    //This is guaranteed to be a vregister not an immediate value, if const propagation has run
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
    struct lox_ir_ll_operand true_value_reg = alloc_new_v_register(lllil, false);
    struct lox_ir_ll_operand false_value_reg = alloc_new_v_register(lllil, false);

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

static struct lox_ir_ll_operand emit_negate_lox(
        struct lllil * lllil,
        struct lox_ir_ll_operand unary_operand //Expect to be in register
) {
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
        struct lox_ir_ll_operand unary_operand //Expect to be in register
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

static struct lox_ir_ll_operand alloc_new_v_register(struct lllil * lllil, bool is_float) {
    return V_REG_TO_OPERAND(alloc_v_register_lox_ir(lllil->lox_ir, is_float));
}

static struct lox_ir_ll_operand emit_lox_any_to_native_cast(
        struct lllil *lllil,
        struct lox_ir_ll_operand input //Expect v_register
) {
    struct v_register return_value_vreg = alloc_v_register_lox_ir(lllil->lox_ir, false);

    emit_function_call_with_return_value_ll_lox_ir(
            lllil,
            &any_to_native_cast_jit_runime,
            return_value_vreg,
            1,
            input
    );

    return V_REG_TO_OPERAND(return_value_vreg);
}

static struct lox_ir_ll_operand emit_lox_number_to_native_i64_cast(
        struct lllil * lllil,
        struct lox_ir_ll_operand input //Expect v_regsiter
) {
    emit_unary_ll_lox_ir(lllil, input, UNARY_LL_LOX_IR_F64_TO_I64_CAST);
    return input;
}

static struct lox_ir_ll_operand emit_lox_struct_instance_to_native(
        struct lllil * lllil,
        struct lox_ir_ll_operand input
) {
    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native(lllil, input);

    emit_load_at_offset_ll_lox_ir(
            lllil,
            native_object_ptr,
            offsetof(struct struct_instance_object, fields)
    );

    return native_object_ptr;
}

static struct lox_ir_ll_operand emit_lox_string_to_native(
        struct lllil * lllil,
        struct lox_ir_ll_operand input //Expect v register
) {
    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native(lllil, input);

    emit_load_at_offset_ll_lox_ir(
            lllil,
            native_object_ptr,
            offsetof(struct string_object, chars)
    );

    return input;
}

static struct lox_ir_ll_operand emit_lox_array_to_native(
        struct lllil * lllil,
        struct lox_ir_ll_operand input
) {
    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native(lllil, input);

    emit_load_at_offset_ll_lox_ir(
            lllil,
            native_object_ptr,
            offsetof(struct array_object, values) + offsetof(struct lox_arraylist, in_use)
    );

    return input;
}

//Lox false value = 0x7ffc000000000002
//Lox true value = 0x7ffc000000000003
//Native true value = 0x01
//Native true value = 0x00
//Native bool value = Lox bool value - 0x7ffc000000000002
static struct lox_ir_ll_operand emit_lox_boolean_to_native(
        struct lllil * lllil,
        struct lox_ir_ll_operand input //Expect v_regsiter
) {
    emit_isub_ll_lox_ir(lllil, input, IMMEDIATE_TO_OPERAND(TRUE_VALUE));
    return input;
}

static struct lox_ir_ll_operand emit_lox_object_ptr_to_native(
        struct lllil * lllil,
        struct lox_ir_ll_operand input //Expect v register
) {
    struct v_register or_fsb_qfn_reg = alloc_v_register_lox_ir(lllil->lox_ir, false);

    emit_move_ll_lox_ir(
            lllil,
            V_REG_TO_OPERAND(or_fsb_qfn_reg),
            IMMEDIATE_TO_OPERAND(~(FLOAT_SIGN_BIT | FLOAT_QNAN))
    );

    //See AS_OBJECT macro in types.h
    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_LOGICAL_AND,
            input,
            IMMEDIATE_TO_OPERAND(~(FLOAT_SIGN_BIT | FLOAT_QNAN))
    );

    return input;
}