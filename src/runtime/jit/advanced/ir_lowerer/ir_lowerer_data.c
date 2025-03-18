#include "ir_lowerer_data.h"

typedef struct lox_ir_ll_operand(* lowerer_lox_ir_data_t)(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_v_register(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_global(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_constant(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_unary(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_cast(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_struct(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_length(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_guard(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_binary(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_call(struct lllil_control*, struct lox_ir_data_node*, struct v_register*);

static struct lox_ir_ll_operand emit_not_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_not_native(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_negate_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_negate_native(struct lllil_control*, struct lox_ir_data_node*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand alloc_new_v_register(struct lllil_control *lllil, bool is_float);
static struct lox_ir_ll_operand emit_lox_any_to_native_cast(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_number_to_native_i64_cast(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_boolean_to_native(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_string_to_native(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_array_to_native(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_struct_instance_to_native(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_object_ptr_to_lox_native(struct lllil_control *lllil, struct lox_ir_ll_operand input);
static struct lox_ir_ll_operand emit_native_i64_to_lox_number(struct lllil_control *lllil, struct lox_ir_ll_operand input);
static struct lox_ir_ll_operand emit_native_bool_to_lox_bool(struct lllil_control *lllil, struct lox_ir_ll_operand input);
static struct lox_ir_ll_operand emit_native_struct_instance_to_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_native_array_to_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_native_struct_instance_to_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_native_string_to_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field_escapes(struct lllil_control*, struct lox_ir_data_get_struct_field_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field_doest_not_escape(struct lllil_control*, struct lox_ir_data_get_struct_field_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_struct_escapes(struct lllil_control*, struct lox_ir_data_initialize_struct_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_struct_does_not_escape(struct lllil_control*, struct lox_ir_data_initialize_struct_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array_escapes(struct lllil_control*, struct lox_ir_data_initialize_array_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array_does_not_escape(struct lllil_control*, struct lox_ir_data_initialize_array_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element_does_not_escape(struct lllil_control*, struct lox_ir_data_get_array_element_node*, struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element_escapes(struct lllil_control*, struct lox_ir_data_get_array_element_node*, struct v_register*);
static struct lox_ir_ll_operand emit_binary_sub(struct lllil_control*, lox_ir_type_t, struct lox_ir_ll_operand, struct lox_ir_ll_operand, struct lox_ir_data_binary_node*, struct v_register*);
static struct lox_ir_ll_operand emit_binary_add(struct lllil_control*,lox_ir_type_t, struct lox_ir_ll_operand,
        struct lox_ir_ll_operand,lox_ir_type_t,lox_ir_type_t,struct lox_ir_data_binary_node*, struct v_register*);
static struct lox_ir_ll_operand emit_binary_div(struct lllil_control*, lox_ir_type_t, struct lox_ir_ll_operand, struct lox_ir_ll_operand, struct v_register*);
static struct lox_ir_ll_operand emit_binary_mul(struct lllil_control*, lox_ir_type_t, struct lox_ir_ll_operand, struct lox_ir_ll_operand, struct v_register*);
static struct lox_ir_ll_operand emit_binary_and(struct lllil_control*, struct lox_ir_ll_operand, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_binary_op(struct lllil_control*, binary_operator_type_ll_lox_ir, struct lox_ir_ll_operand, struct lox_ir_ll_operand, struct v_register*);
static struct lox_ir_ll_operand emit_lox_generic_binary(struct lllil_control*, bytecode_t, struct lox_ir_ll_operand, struct lox_ir_ll_operand, struct v_register*);
static void emit_string_concat(struct lllil_control*,struct lox_ir_ll_operand,struct lox_ir_ll_operand,struct lox_ir_ll_operand,lox_ir_type_t,lox_ir_type_t,lox_ir_type_t);
static bool is_binary_inc(struct lox_ir_data_binary_node *);
static bool is_binary_dec(struct lox_ir_data_binary_node *);
static void emit_native_to_lox(struct lllil_control*,struct lox_ir_ll_operand,lox_ir_type_t,lox_ir_type_t);
static void emit_lox_to_native(struct lllil_control*,struct lox_ir_ll_operand,lox_ir_type_t,lox_ir_type_t);
static struct lox_ir_ll_operand emit_operand_to_register(struct lllil_control*,struct lox_ir_ll_operand,bool);
static struct lox_ir_ll_operand copy_into_new_register(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand lowerer_lox_ir_data_lox_function_call(struct lllil_control*,struct lox_ir_data_function_call_node*,struct v_register*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_native_function_call(struct lllil_control*,struct lox_ir_data_function_call_node*,struct v_register*);

static bool is_struct_field_fp(struct lox_ir_data_node *, char * field_name);
static bool parent_will_have_unwanted_side_effect(struct lllil_control*, struct lox_ir_ll_operand input, struct lox_ir_data_node *parent);

extern struct struct_instance_object * alloc_struct_instance_gc_alg(struct struct_definition_object*);
extern void runtime_panic(char * format, ...);
extern uint64_t any_to_native_cast_jit_runime(lox_value_t);
extern struct array_object * alloc_array_gc_alg(int);
extern void * string_concatenate_jit_runime(void*,void*,lox_ir_type_t,lox_ir_type_t,lox_ir_type_t);
extern void call_lox_function_jit_runtime(struct function_object *, bool);

extern lox_value_t addition_lox(lox_value_t a, lox_value_t b);

lowerer_lox_ir_data_t lowerer_lox_ir_by_data_node[] = {
        [LOX_IR_DATA_NODE_INITIALIZE_STRUCT] = lowerer_lox_ir_data_initialize_struct,
        [LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT] = lowerer_lox_ir_data_get_array_element,
        [LOX_IR_DATA_NODE_INITIALIZE_ARRAY] = lowerer_lox_ir_data_initialize_array,
        [LOX_IR_DATA_NODE_GET_STRUCT_FIELD] = lowerer_lox_ir_data_get_struct_field,
        [LOX_IR_DATA_NODE_GET_ARRAY_LENGTH] = lowerer_lox_ir_data_get_array_length,
        [LOX_IR_DATA_NODE_GET_V_REGISTER] = lowerer_lox_ir_data_get_v_register,
        [LOX_IR_DATA_NODE_GET_GLOBAL] = lowerer_lox_ir_data_get_global,
        [LOX_IR_DATA_NODE_CONSTANT] = lowerer_lox_ir_data_constant,
        [LOX_IR_DATA_NODE_BINARY] = lowerer_lox_ir_data_binary,
        [LOX_IR_DATA_NODE_UNARY] = lowerer_lox_ir_data_unary,
        [LOX_IR_DATA_NODE_GUARD] = lowerer_lox_ir_data_guard,
        [LOX_IR_DATA_NODE_CAST] = lowerer_lox_ir_data_cast,
        [LOX_IR_DATA_NODE_CALL] = lowerer_lox_ir_data_call,

        [LOX_IR_DATA_NODE_GET_SSA_NAME] = NULL,
        [LOX_IR_DATA_NODE_GET_LOCAL] = NULL,
        [LOX_IR_DATA_NODE_PHI] = NULL,
};

struct lox_ir_ll_operand lower_lox_ir_data(
        struct lllil_control * lllil_control,
        struct lox_ir_data_node * parent_node,
        struct lox_ir_data_node * data_node,
        struct v_register * result
) {
    lowerer_lox_ir_data_t lowerer_lox_ir_data = lowerer_lox_ir_by_data_node[data_node->type];

    if (lowerer_lox_ir_data == NULL) {
        runtime_panic("Unexpected data control %i in ir_lowerer_data", data_node->type);
    }

    struct lox_ir_ll_operand operand_result = lowerer_lox_ir_data(lllil_control, data_node, result);

    if (parent_will_have_unwanted_side_effect(lllil_control, operand_result, parent_node)) {
        operand_result = copy_into_new_register(lllil_control, operand_result);
    }

    return operand_result;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_binary(
        struct lllil_control * control,
        struct lox_ir_data_node * data_node,
        struct v_register * result
) {
    struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) data_node;
    lox_ir_type_t binary_produced_type = binary->data.produced_type->type;
    struct lox_ir_ll_operand right = lower_lox_ir_data(control, data_node, binary->right, NULL);
    struct lox_ir_ll_operand left = lower_lox_ir_data(control, data_node, binary->left, NULL);

    if (binary_produced_type == LOX_IR_TYPE_LOX_ANY && !is_comparation_bytecode_instruction(binary->operator)) {
        return emit_lox_generic_binary(control, binary->operator, left, right, result);
    }

    switch (binary->operator) {
        case OP_ADD:
            return emit_binary_add(control, binary_produced_type, left, right, binary->left->produced_type->type,
                binary->right->produced_type->type, binary, result);
        case OP_SUB:
            return emit_binary_sub(control, binary_produced_type, left, right, binary, result);
        case OP_MUL:
            return emit_binary_mul(control, binary_produced_type, left, right, result);
        case OP_DIV:
            return emit_binary_div(control, binary_produced_type, left, right, result);
        case OP_MODULO:
            return emit_binary_op(control, BINARY_LL_LOX_IR_MOD, left, right, result);
        case OP_BINARY_OP_AND:
            return emit_binary_op(control, BINARY_LL_LOX_IR_AND, left, right, result);
        case OP_BINARY_OP_OR:
            return emit_binary_op(control, BINARY_LL_LOX_IR_OR, left, right, result);
        case OP_LEFT_SHIFT:
            return emit_binary_op(control, BINARY_LL_LOX_IR_LEFT_SHIFT, left, right, result);
        case OP_RIGHT_SHIFT:
            return emit_binary_op(control, BINARY_LL_LOX_IR_RIGHT_SHIFT, left, right, result);
        case OP_GREATER:
            emit_comparation_ll_lox_ir(control, COMPARATION_LL_LOX_IR_GREATER, left, right);
            return FLAGS_OPERAND();
        case OP_LESS:
            emit_comparation_ll_lox_ir(control, COMPARATION_LL_LOX_IR_LESS, left, right);
            return FLAGS_OPERAND();
        case OP_EQUAL:
            emit_comparation_ll_lox_ir(control, COMPARATION_LL_LOX_IR_EQ, left, right);
            return FLAGS_OPERAND();
        default:
            //TODO Panic
    }
}

static struct lox_ir_ll_operand emit_lox_generic_binary(
        struct lllil_control * control,
        bytecode_t operator,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right,
        struct v_register * result
) {
    if(operator != OP_ADD){
        //TODO Panic
    }

    struct v_register return_value = result != NULL ?
            *result : alloc_v_register_lox_ir(control->lllil->lox_ir, false);

    emit_function_call_with_return_value_ll_lox_ir(
            control,
            addition_lox,
            "addition_lox",
            return_value,
            2,
            left,
            right
    );

    return V_REG_TO_OPERAND(return_value);
}

static struct lox_ir_ll_operand emit_binary_op(
        struct lllil_control * control,
        binary_operator_type_ll_lox_ir op,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right,
        struct v_register * expected_result
) {
    struct lox_ir_ll_operand result = expected_result == NULL ?
            alloc_new_v_register(control, false) : V_REG_TO_OPERAND(*expected_result);

    emit_binary_ll_lox_ir(control, op, result, left, right);
    return result;
}

static struct lox_ir_ll_operand emit_binary_and(
        struct lllil_control * control,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right
) {
    struct lox_ir_ll_operand result = alloc_new_v_register(control, false);

    emit_binary_ll_lox_ir(
            control,
            BINARY_LL_LOX_IR_AND,
            result,
            left,
            right
    );

    return result;
}

static struct lox_ir_ll_operand emit_binary_sub(
        struct lllil_control * control,
        lox_ir_type_t type_to_produce,
        struct lox_ir_ll_operand left_operand,
        struct lox_ir_ll_operand right_operand,
        struct lox_ir_data_binary_node * binary,
        struct v_register * expected_result
) {
    bool is_fp = type_to_produce == LOX_IR_TYPE_F64;
    bool is_lox_i64 = type_to_produce == LOX_IR_TYPE_LOX_I64;
    bool is_native_i64 = type_to_produce == LOX_IR_TYPE_NATIVE_I64;

    struct lox_ir_ll_operand result = expected_result == NULL ?
            alloc_new_v_register(control, is_fp) : V_REG_TO_OPERAND(*expected_result);

    if (is_native_i64 && is_binary_dec(binary)) {
        emit_unary_ll_lox_ir(control, left_operand.type == LOX_IR_LL_OPERAND_IMMEDIATE ? right_operand : left_operand,
                             UNARY_LL_LOX_IR_DEC);
    } else {
        emit_binary_ll_lox_ir(
                control,
                is_fp? BINARY_LL_LOX_IR_FSUB : BINARY_LL_LOX_IR_ISUB,
                result,
                left_operand,
                right_operand
        );
    }

    return result;
}

static struct lox_ir_ll_operand emit_binary_div(
        struct lllil_control * control,
        lox_ir_type_t type_to_produce,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right,
        struct v_register * expected_result
) {
    bool is_fp = type_to_produce == LOX_IR_TYPE_F64;
    struct lox_ir_ll_operand result = expected_result != NULL ?
            V_REG_TO_OPERAND(*expected_result) : alloc_new_v_register(control, is_fp);

    emit_binary_ll_lox_ir(
            control,
            is_fp? BINARY_LL_LOX_IR_FDIV : BINARY_LL_LOX_IR_IDIV,
            result,
            left,
            right
    );

    return result;
}

static struct lox_ir_ll_operand emit_binary_mul(
        struct lllil_control * control,
        lox_ir_type_t type_to_produce,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right,
        struct v_register * expected_result
) {
    bool is_fp = type_to_produce == LOX_IR_TYPE_F64;
    struct lox_ir_ll_operand result = expected_result != NULL ?
            V_REG_TO_OPERAND(*expected_result) : alloc_new_v_register(control, is_fp);

    emit_binary_ll_lox_ir(
            control,
            is_fp? BINARY_LL_LOX_IR_FMUL : BINARY_LL_LOX_IR_IMUL,
            result,
            left,
            right
    );

    return result;
}

static struct lox_ir_ll_operand emit_binary_add(
        struct lllil_control * control,
        lox_ir_type_t type_to_produce,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right,
        lox_ir_type_t left_type,
        lox_ir_type_t right_type,
        struct lox_ir_data_binary_node * binary,
        struct v_register * expected_result
) {
    bool is_string = IS_STRING_LOX_IR_TYPE(type_to_produce);
    bool is_fp = type_to_produce == LOX_IR_TYPE_F64;
    bool is_i64 = type_to_produce == LOX_IR_TYPE_NATIVE_I64;

    struct lox_ir_ll_operand result = expected_result != NULL ?
            V_REG_TO_OPERAND(*expected_result) : alloc_new_v_register(control, is_fp);

    if (is_string) {
        emit_string_concat(control, result, left, right, left_type, right_type, type_to_produce);
    } else if (is_fp) {
        emit_binary_ll_lox_ir(control, BINARY_LL_LOX_IR_FADD, result, left, right);
    } else if (is_i64 && is_binary_inc(binary)){
        emit_unary_ll_lox_ir(control, left.type == LOX_IR_LL_OPERAND_IMMEDIATE ? right : left, UNARY_LL_LOX_IR_INC);
    } else if (is_i64) {
        emit_binary_ll_lox_ir(control, BINARY_LL_LOX_IR_IADD, result, left, right);
    }

    return result;
}

static void emit_string_concat(
        struct lllil_control * control,
        struct lox_ir_ll_operand result,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right,
        lox_ir_type_t type_left,
        lox_ir_type_t type_right,
        lox_ir_type_t type_to_produce
) {
    //TODO Optimize by inlining specializing string concatenation code
    emit_function_call_with_return_value_ll_lox_ir(
            control,
            &string_concatenate_jit_runime,
            "string_concatenate_jit_runime",
            result.v_register,
            5,
            left,
            right,
            IMMEDIATE_TO_OPERAND(type_left),
            IMMEDIATE_TO_OPERAND(type_right),
            IMMEDIATE_TO_OPERAND(type_to_produce)
    );
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_guard(
        struct lllil_control * lllil_control,
        struct lox_ir_data_node * data_node,
        struct v_register * result
) {
    struct lox_ir_data_guard_node * guard_node = (struct lox_ir_data_guard_node *) data_node;
    struct lox_ir_ll_operand guard_input = lower_lox_ir_data(lllil_control, data_node, guard_node->guard.value, result);

    return emit_guard_ll_lox_ir(
            lllil_control,
            guard_input,
            guard_node->guard
    );
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_length(
        struct lllil_control * control,
        struct lox_ir_data_node * data_node,
        struct v_register * expected_result
) {
    struct lox_ir_data_get_array_length * get_length = (struct lox_ir_data_get_array_length *) data_node;

    struct lox_ir_ll_operand array_instance = lower_lox_ir_data(control, data_node, get_length->instance, expected_result);

    struct v_register result = expected_result != NULL ?
            *expected_result : alloc_v_register_lox_ir(control->lllil->lox_ir, false);

    return emit_get_array_length_ll_lox_ir(control, array_instance, get_length->escapes, result);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element(
        struct lllil_control * lllil_control,
        struct lox_ir_data_node * data_node,
        struct v_register * result
) {
    struct lox_ir_data_get_array_element_node * get_arr_element = (struct lox_ir_data_get_array_element_node *) data_node;

    if (!get_arr_element->escapes) {
        return lowerer_lox_ir_data_get_array_element_does_not_escape(lllil_control, get_arr_element, result);
    } else {
        return lowerer_lox_ir_data_get_array_element_escapes(lllil_control, get_arr_element, result);
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element_does_not_escape(
        struct lllil_control * lllil,
        struct lox_ir_data_get_array_element_node * node,
        struct v_register * result
) {
    struct lox_ir_ll_operand instance = lower_lox_ir_data(lllil, &node->data, node->index, result);
    struct lox_ir_ll_operand index = lower_lox_ir_data(lllil, &node->data, node->index, NULL);

    if (node->requires_range_check) {
        emit_range_check_ll_lox_ir(lllil, instance, index, node->escapes);
    }

    if (index.type == LOX_IR_LL_OPERAND_IMMEDIATE) {
        emit_load_at_offset_ll_lox_ir(
                lllil,
                instance,
                instance,
                sizeof(uint64_t) * index.immedate
        );
    } else {
        emit_binary_ll_lox_ir(
                lllil,
                BINARY_LL_LOX_IR_IMUL,
                index,
                IMMEDIATE_TO_OPERAND(sizeof(uint64_t)),
                index
        );
        emit_binary_ll_lox_ir(
                lllil,
                BINARY_LL_LOX_IR_IADD,
                instance,
                instance,
                index
        );
        emit_load_at_offset_ll_lox_ir(
                lllil,
                instance,
                instance,
                0
        );
    }

    return instance;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element_escapes(
        struct lllil_control * lllil,
        struct lox_ir_data_get_array_element_node * node,
        struct v_register * result
) {
    struct lox_ir_ll_operand instance = lower_lox_ir_data(lllil, &node->data, node->index, result);
    struct lox_ir_ll_operand index = lower_lox_ir_data(lllil, &node->data, node->index, NULL);

    if (node->requires_range_check) {
        emit_range_check_ll_lox_ir(lllil, instance, index, node->escapes);
    }

    if (index.type == LOX_IR_LL_OPERAND_IMMEDIATE) {
        emit_load_at_offset_ll_lox_ir(
                lllil,
                instance,
                instance,
                offsetof(struct array_object, values)
                + offsetof(struct array_object, values)
                + sizeof(uint64_t) * index.immedate
        );
    } else {
        //Array element will contain the poniter value of the array values
        emit_load_at_offset_ll_lox_ir(
                lllil,
                instance,
                instance,
                offsetof(struct array_object, values) + offsetof(struct array_object, values)
        );
        emit_binary_ll_lox_ir(
                lllil,
                BINARY_LL_LOX_IR_IMUL,
                instance,
                IMMEDIATE_TO_OPERAND(sizeof(uint64_t)),
                index
        );
        emit_load_at_offset_ll_lox_ir(
                lllil,
                instance,
                instance,
                0
        );
    }

    return instance;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array(
        struct lllil_control * lllil_control,
        struct lox_ir_data_node * node,
        struct v_register * result
) {
    struct lox_ir_data_initialize_array_node * init_arr = (struct lox_ir_data_initialize_array_node *) node;
    size_t array_size = init_arr->n_elements * sizeof(uint64_t);

    if (!init_arr->escapes && array_size < 512) {
        return lowerer_lox_ir_data_initialize_array_does_not_escape(lllil_control, init_arr);
    } else {
        return lowerer_lox_ir_data_initialize_array_escapes(lllil_control, init_arr, result);
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array_does_not_escape(
        struct lllil_control * lllil,
        struct lox_ir_data_initialize_array_node * init_array
) {
    size_t array_size = sizeof(int) + sizeof(uint64_t) * init_array->n_elements;
    int stack_slot = allocate_stack_slot_lllil(lllil->lllil, lllil->control_node_to_lower->block, array_size);

    //Move array size
    emit_move_ll_lox_ir(
            lllil,
            STACKSLOT_TO_OPERAND(stack_slot, 0),
            IMMEDIATE_TO_OPERAND(array_size)
    );

    for (int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++) {
        struct lox_ir_ll_operand array_element_reg = lower_lox_ir_data(lllil, &init_array->data,
                init_array->elememnts[i], NULL);
        size_t element_field_offset = sizeof(int) + i * sizeof(uint64_t);

        emit_move_ll_lox_ir(
                lllil,
                STACKSLOT_TO_OPERAND(stack_slot, element_field_offset),
                array_element_reg
        );
    }

    return STACKSLOT_TO_OPERAND(stack_slot, sizeof(int));
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array_escapes(
        struct lllil_control * lllil_control,
        struct lox_ir_data_initialize_array_node * init_array,
        struct v_register * result
) {
    struct lox_ir_ll_operand array_instance_operand = result != NULL ?
            V_REG_TO_OPERAND(*result) : alloc_new_v_register(lllil_control, false);
    int n_elements = init_array->n_elements;

    emit_function_call_with_return_value_ll_lox_ir(
            lllil_control,
            alloc_array_gc_alg,
            "alloc_array_gc_alg",
            array_instance_operand.v_register,
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) init_array->n_elements)
    );

    emit_binary_ll_lox_ir(
            lllil_control,
            BINARY_LL_LOX_IR_IADD,
            array_instance_operand,
            array_instance_operand,
            IMMEDIATE_TO_OPERAND(sizeof(struct lox_arraylist))
    );

    for(int i = 0; i < n_elements && !init_array->empty_initialization; i++) {
        struct lox_ir_ll_operand struct_element_reg = lower_lox_ir_data(lllil_control, &init_array->data,
                init_array->elememnts[i],NULL);

        emit_store_at_offset_ll_lox_ir(
                lllil_control,
                array_instance_operand,
                sizeof(uint64_t) * i,
                struct_element_reg
        );
    }

    return array_instance_operand;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_struct(
        struct lllil_control * lllil,
        struct lox_ir_data_node * data_node,
        struct v_register * result
) {
    struct lox_ir_data_initialize_struct_node * init_struct = (struct lox_ir_data_initialize_struct_node *) data_node;

    if (!init_struct->escapes) {
        return lowerer_lox_ir_data_initialize_struct_does_not_escape(lllil, init_struct);
    } else {
        return lowerer_lox_ir_data_initialize_struct_escapes(lllil, init_struct);
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_struct_escapes(
        struct lllil_control * lllil,
        struct lox_ir_data_initialize_struct_node * init_node
) {
    struct lox_ir_ll_operand struct_instance_operand = alloc_new_v_register(lllil, false);
    struct struct_definition_object * definition = init_node->definition;

    emit_function_call_with_return_value_ll_lox_ir(
            lllil,
            alloc_struct_instance_gc_alg,
            "alloc_struct_instance_gc_alg",
            struct_instance_operand.v_register,
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) init_node->definition)
    );

    for (int i = 0; i < init_node->definition->n_fields; i++) {
        struct string_object * field_name = definition->field_names[definition->n_fields - i - 1];

        struct lox_ir_ll_operand struct_element_reg = lower_lox_ir_data(lllil, &init_node->data,
                init_node->fields_nodes[i], NULL);

        emit_function_call_ll_lox_ir(
                lllil,
                put_hash_table,
                "put_hash_table",
                3,
                struct_instance_operand,
                IMMEDIATE_TO_OPERAND((uint64_t) field_name),
                struct_element_reg
        );
    }

    return struct_instance_operand;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_struct_does_not_escape(
        struct lllil_control * lllil,
        struct lox_ir_data_initialize_struct_node * init_node
) {
    struct struct_definition_object * definition = init_node->definition;
    size_t struct_size = sizeof(uint64_t) * definition->n_fields;
    int stack_slot = allocate_stack_slot_lllil(lllil->lllil, lllil->control_node_to_lower->block, struct_size);

    for (int i = 0; i < definition->n_fields; i++) {
        struct string_object * field_name = definition->field_names[definition->n_fields - i - 1];
        struct lox_ir_ll_operand struct_element_reg = lower_lox_ir_data(lllil, &init_node->data,
                init_node->fields_nodes[i], NULL);
        size_t field_struct_field_offset = i * sizeof(uint64_t);

        emit_move_ll_lox_ir(
                lllil,
                STACKSLOT_TO_OPERAND(stack_slot, field_struct_field_offset),
                struct_element_reg
        );
    }

    return STACKSLOT_TO_OPERAND(stack_slot, 0);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field(
        struct lllil_control * lllil,
        struct lox_ir_data_node * data_node,
        struct v_register * result
) {
    struct lox_ir_data_get_struct_field_node  * get_struct_field = (struct lox_ir_data_get_struct_field_node *) data_node;

    if(!get_struct_field->escapes) {
        return lowerer_lox_ir_data_get_struct_field_doest_not_escape(lllil, get_struct_field, result);
    } else {
        return lowerer_lox_ir_data_get_struct_field_escapes(lllil, get_struct_field, result);
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field_doest_not_escape(
        struct lllil_control * lllil,
        struct lox_ir_data_get_struct_field_node * get_struct_field,
        struct v_register * result
) {
    //If it escapes, it should produce a native pointer to struct instance
    struct lox_ir_ll_operand struct_instance = lower_lox_ir_data(lllil, &get_struct_field->data,
            get_struct_field->instance, NULL);

    //We are guaranteed that struct operation nodes that escapes have a struct definition type. This is guaranteed in escape_analysis
    //in phi node processing
    struct struct_definition_object * definition = get_struct_field->instance->produced_type->value.struct_instance->definition;
    bool is_field_fp = is_struct_field_fp(get_struct_field->instance, get_struct_field->field_name->chars);

    int field_offset = get_offset_field_struct_definition_ll_lox_ir(definition, get_struct_field->field_name->chars);

    struct lox_ir_ll_operand get_struct_field_reg_result = result != NULL ?
            V_REG_TO_OPERAND(*result) : V_REG_TO_OPERAND(alloc_v_register_lox_ir(lllil->lllil->lox_ir, is_field_fp));

    emit_load_at_offset_ll_lox_ir(
            lllil,
            get_struct_field_reg_result,
            struct_instance,
            field_offset
    );

    return get_struct_field_reg_result;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field_escapes(
        struct lllil_control * lllil,
        struct lox_ir_data_get_struct_field_node * get_struct_field,
        struct v_register * result
) {
    struct lox_ir_ll_operand struct_instance = lower_lox_ir_data(lllil, &get_struct_field->data,
            get_struct_field->instance, NULL);

    bool is_field_fp = is_struct_field_fp(get_struct_field->instance, get_struct_field->field_name->chars);
    struct lox_ir_ll_operand get_struct_field_reg_result = result != NULL ?
            V_REG_TO_OPERAND(result) : V_REG_TO_OPERAND(alloc_v_register_lox_ir(lllil->lllil->lox_ir, is_field_fp));

    emit_function_call_with_return_value_ll_lox_ir(
            lllil,
            get_hash_table,
            "get_hash_table",
            get_struct_field_reg_result.v_register,
            2,
            struct_instance,
            IMMEDIATE_TO_OPERAND((uint64_t) get_struct_field->field_name->chars)
    );

    return get_struct_field_reg_result;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_call(
        struct lllil_control * lllil,
        struct lox_ir_data_node * data_node,
        struct v_register * result
) {
    struct lox_ir_data_function_call_node * call = (struct lox_ir_data_function_call_node *) data_node;

    if (call->is_native) {
        return lowerer_lox_ir_data_native_function_call(lllil, call, result);
    } else {
        return lowerer_lox_ir_data_lox_function_call(lllil, call, result);
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_native_function_call(
        struct lllil_control * lllil,
        struct lox_ir_data_function_call_node * call,
        struct v_register * result
) {
    bool return_value_used = lllil->control_node_to_lower->type != LOX_IR_CONTROL_NODE_DATA;

    struct lox_ir_ll_operand arguments[call->n_arguments];
    for (int i = 0; i < call->n_arguments; i++) {
        struct lox_ir_ll_operand * function_arg = LOX_MALLOC(LOX_IR_ALLOCATOR(lllil->lllil->lox_ir), sizeof(struct lox_ir_ll_operand));
        arguments[i] = lower_lox_ir_data(lllil, &call->data, call->arguments[i], result);
    }

    struct lox_ir_ll_operand return_value_v_reg;

    if (return_value_used) {
        //TODO Is float
        return_value_v_reg = result != NULL ?
                V_REG_TO_OPERAND(*result) :
                alloc_new_v_register(lllil, false);

        emit_function_call_with_return_value_manual_args_ll_lox_ir(
                lllil,
                call->native_function->native_jit_fn,
                call->native_function->name,
                return_value_v_reg.v_register,
                call->n_arguments,
                arguments
        );
    } else {
        emit_function_call_manual_args_ll_lox_ir(
                lllil,
                call->native_function->native_jit_fn,
                call->native_function->name,
                call->n_arguments,
                arguments
        );
    }

    return return_value_v_reg;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_lox_function_call(
        struct lllil_control * lllil,
        struct lox_ir_data_function_call_node * call,
        struct v_register * result
) {
    bool return_value_used = lllil->control_node_to_lower->type != LOX_IR_CONTROL_NODE_DATA;

    struct ptr_arraylist arguments;
    init_ptr_arraylist(&arguments, LOX_IR_ALLOCATOR(lllil->lllil->lox_ir));

    for (int i = 0; i < call->n_arguments; i++) {
        struct lox_ir_ll_operand * function_arg = LOX_MALLOC(LOX_IR_ALLOCATOR(lllil->lllil->lox_ir), sizeof(struct lox_ir_ll_operand));
        *function_arg = lower_lox_ir_data(lllil, &call->data, call->arguments[i], result);
        append_ptr_arraylist(&arguments, function_arg);
    }

    struct lox_ir_ll_operand return_value_v_reg;

    if (return_value_used) {
        //TODO IS float?
        return_value_v_reg = result != NULL ?
                             V_REG_TO_OPERAND(*result) :
                             alloc_new_v_register(lllil, false);

        emit_function_call_with_return_value_ll_lox_ir(
                lllil,
                call_lox_function_jit_runtime,
                "call_lox_function_jit_runtime",
                return_value_v_reg.v_register,
                2,
                IMMEDIATE_TO_OPERAND((uint64_t) call->lox_function.function),
                IMMEDIATE_TO_OPERAND((uint64_t) call->lox_function.is_parallel)
        );
    } else {
        emit_function_call_ll_lox_ir(
                lllil,
                call_lox_function_jit_runtime,
                "call_lox_function_jit_runtime",
                2,
                IMMEDIATE_TO_OPERAND((uint64_t) call->lox_function.function),
                IMMEDIATE_TO_OPERAND((uint64_t) call->lox_function.is_parallel)
        );
    }

    return return_value_v_reg;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_cast(
        struct lllil_control * lllil,
        struct lox_ir_data_node * data_node,
        struct v_register * result
) {
    //cast_insertion guarantees that cast nodes won't have as input a const node, so when we call lower_lox_ir_data(csat->to_cast)
    //it will be returned as a v register
    struct lox_ir_data_cast_node * cast = (struct lox_ir_data_cast_node *) data_node;
    struct lox_ir_ll_operand to_cast_input = lower_lox_ir_data(lllil, data_node, cast->to_cast, result);

    lox_ir_type_t actual_type = cast->to_cast->produced_type->type;
    lox_ir_type_t expected_type = cast->data.produced_type->type;

    if (is_lox_lox_ir_type(actual_type)) {
        emit_lox_to_native(lllil, to_cast_input, actual_type, expected_type);
    } else {
        emit_native_to_lox(lllil, to_cast_input, actual_type, expected_type);
    }

    return to_cast_input;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_global(
        struct lllil_control * lllil,
        struct lox_ir_data_node * data_node,
        struct v_register * result
) {
    struct lox_ir_data_get_global_node * get_global = (struct lox_ir_data_get_global_node *) data_node;
    struct v_register global_v_register = result != NULL ?
            *result : alloc_v_register_lox_ir(lllil->lllil->lox_ir, false);

    emit_function_call_with_return_value_ll_lox_ir(
            lllil,
            &get_hash_table,
            "get_hash_table",
            global_v_register,
            2,
            IMMEDIATE_TO_OPERAND((uint64_t) &get_global->package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) &get_global->name)
    );

    return V_REG_TO_OPERAND(global_v_register);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_v_register(
        struct lllil_control * lllil,
        struct lox_ir_data_node * node,
        struct v_register * _
) {
    struct lox_ir_data_get_v_register_node * get_v_reg = (struct lox_ir_data_get_v_register_node *) node;
    return V_REG_TO_OPERAND(get_v_reg->v_register);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_constant(
        struct lllil_control * lllil,
        struct lox_ir_data_node * node,
        struct v_register * result
) {
    struct lox_ir_data_constant_node * const_node = (struct lox_ir_data_constant_node *) node;
    return IMMEDIATE_TO_OPERAND(const_node->value);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_unary(
        struct lllil_control * lllil,
        struct lox_ir_data_node * data,
        struct v_register * result
) {
    struct lox_ir_data_unary_node * unary = (struct lox_ir_data_unary_node *) data;
    struct lox_ir_data_node * unary_operand_node = unary->operand;
    //This is guaranteed to be a vregister not an immediate value, if const propagation has run
    struct lox_ir_ll_operand unary_operand = lower_lox_ir_data(lllil, data, unary_operand_node, result);
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
}

//True value_node:  0x7ffc000000000003
//False value_node: 0x7ffc000000000002
//value1 = value0 + ((TRUE - value0) + (FALSE - value0))
//value1 = - value0 + TRUE + FALSE
//value1 = (TRUE - value_node) + FALSE
static struct lox_ir_ll_operand emit_not_lox(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand unary_operand //Expect to be in register
) {
    struct lox_ir_ll_operand true_value_reg = alloc_new_v_register(lllil, false);
    struct lox_ir_ll_operand false_value_reg = alloc_new_v_register(lllil, false);

    emit_move_ll_lox_ir(lllil, true_value_reg, IMMEDIATE_TO_OPERAND(TRUE_VALUE));

    emit_binary_ll_lox_ir(lllil, BINARY_LL_LOX_IR_ISUB, unary_operand, true_value_reg, unary_operand);

    emit_move_ll_lox_ir(lllil, false_value_reg, IMMEDIATE_TO_OPERAND(FALSE_VALUE));

    emit_binary_ll_lox_ir(lllil, BINARY_LL_LOX_IR_IADD, unary_operand, unary_operand, false_value_reg);

    return unary_operand;
}

static struct lox_ir_ll_operand emit_not_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand unary_operand //Expect to be in register
) {
    emit_unary_ll_lox_ir(lllil, unary_operand, UNARY_LL_LOX_IR_LOGICAL_NEGATION);
    return unary_operand;
}

static struct lox_ir_ll_operand emit_negate_lox(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand unary_operand //Expect to be in register
) {
    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_XOR,
            unary_operand,
            unary_operand,
            IMMEDIATE_TO_OPERAND(0x8000000000000000)
    );
    return unary_operand;
}

static struct lox_ir_ll_operand emit_negate_native(
        struct lllil_control * lllil,
        struct lox_ir_data_node * unary_operand_node,
        struct lox_ir_ll_operand unary_operand //Expect to be in register
) {
    if (unary_operand_node->produced_type->type == LOX_IR_TYPE_F64) { //Float
        emit_binary_ll_lox_ir(
                lllil,
                BINARY_LL_LOX_IR_XOR,
                unary_operand,
                unary_operand,
                IMMEDIATE_TO_OPERAND(0x8000000000000000)
        );
    } else {
        emit_unary_ll_lox_ir(lllil, unary_operand, UNARY_LL_LOX_IR_NUMBER_NEGATION);
    }

    return unary_operand;
}

static struct lox_ir_ll_operand alloc_new_v_register(struct lllil_control * lllil, bool is_float) {
    return V_REG_TO_OPERAND(alloc_v_register_lox_ir(lllil->lllil->lox_ir, is_float));
}

static struct lox_ir_ll_operand emit_lox_any_to_native_cast(
        struct lllil_control *lllil,
        struct lox_ir_ll_operand input //Expect v_register
) {
    emit_function_call_with_return_value_ll_lox_ir(
            lllil,
            &any_to_native_cast_jit_runime,
            "any_to_native_cast_jit_runime",
            input.v_register,
            1,
            input
    );

    return input;
}

static struct lox_ir_ll_operand emit_native_i64_to_lox_number(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    emit_unary_ll_lox_ir(lllil, input, UNARY_LL_LOX_IR_I64_TO_F64_CAST);
    return input;
}

static struct lox_ir_ll_operand emit_lox_number_to_native_i64_cast(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input //Expect v_regsiter
) {
    emit_unary_ll_lox_ir(lllil, input, UNARY_LL_LOX_IR_F64_TO_I64_CAST);
    return input;
}

static struct lox_ir_ll_operand emit_native_struct_instance_to_lox(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_ISUB,
            input,
            input,
            IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields))
    );

    return emit_lox_object_ptr_to_native_ll_lox_ir(lllil, input);
}

static struct lox_ir_ll_operand emit_lox_struct_instance_to_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native_ll_lox_ir(lllil, input);

    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_IADD,
            native_object_ptr,
            native_object_ptr,
            IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields))
    );

    return native_object_ptr;
}

static struct lox_ir_ll_operand emit_lox_string_to_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input //Expect v register
) {
    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native_ll_lox_ir(lllil, input);

    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_IADD,
            native_object_ptr,
            native_object_ptr,
            IMMEDIATE_TO_OPERAND(offsetof(struct string_object, chars))
    );

    return input;
}

static struct lox_ir_ll_operand emit_native_array_to_lox(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_ISUB,
            input,
            input,
            IMMEDIATE_TO_OPERAND(offsetof(struct array_object, values) + offsetof(struct lox_arraylist, in_use))
    );

    return emit_lox_object_ptr_to_native_ll_lox_ir(lllil, input);
}

static struct lox_ir_ll_operand emit_native_string_to_lox(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    uint64_t offset = offsetof(struct array_object, values) + offsetof(struct lox_arraylist, in_use);

    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_ISUB,
            input,
            input,
            IMMEDIATE_TO_OPERAND(offset)
    );

    return emit_lox_object_ptr_to_native_ll_lox_ir(lllil, input);
}

static struct lox_ir_ll_operand emit_lox_array_to_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native_ll_lox_ir(lllil, input);
    uint64_t offset = offsetof(struct array_object, values) + offsetof(struct lox_arraylist, in_use);

    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_IADD,
            input,
            input,
            IMMEDIATE_TO_OPERAND(offset)
    );

    return input;
}

//Lox false value = 0x7ffc000000000002
//Lox true value = 0x7ffc000000000003
//Native true value = 0x01
//Native true value = 0x00
//Native bool value = Lox bool value - 0x7ffc000000000002
//Lox bool value = Native bool value + 0x7ffc000000000002
static struct lox_ir_ll_operand emit_native_bool_to_lox_bool(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_IADD,
            input,
            input,
            IMMEDIATE_TO_OPERAND(0x7ffc000000000002)
    );

    return input;
}

//Lox false value = 0x7ffc000000000002
//Lox true value = 0x7ffc000000000003
//Native true value = 0x01
//Native true value = 0x00
//Native bool value = Lox bool value - 0x7ffc000000000002
static struct lox_ir_ll_operand emit_lox_boolean_to_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input //Expect v_regsiter
) {
    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_ISUB,
            input,
            input,
            IMMEDIATE_TO_OPERAND(TRUE_VALUE)
    );
    return input;
}

static struct lox_ir_ll_operand emit_object_ptr_to_lox_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_OR,
            input,
            input,
            IMMEDIATE_TO_OPERAND(FLOAT_SIGN_BIT | FLOAT_QNAN)
    );

    return input;
}

static bool is_struct_field_fp(
        struct lox_ir_data_node * struct_instasnce_node,
        char * field_name
) {
    if(struct_instasnce_node->produced_type->type != LOX_IR_TYPE_LOX_STRUCT_INSTANCE &&
        struct_instasnce_node->produced_type->type != LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE) {
        return false;
    }

    struct struct_instance_lox_ir_type * struct_instance_type = struct_instasnce_node->produced_type->value.struct_instance;

    if (struct_instance_type == NULL || struct_instance_type->definition == NULL) {
        return false;
    }

    struct lox_ir_type * field_value_type = get_u64_hash_table(&struct_instance_type->type_by_field_name, (uint64_t) field_name);
    return field_value_type != NULL && field_value_type->type == LOX_IR_TYPE_F64;;
}

static bool is_binary_inc(struct lox_ir_data_binary_node * binary) {
    return binary->operator == OP_ADD
        && binary->data.produced_type->type == LOX_IR_TYPE_NATIVE_I64
        && (binary->left->type == LOX_IR_DATA_NODE_CONSTANT || binary->right->type == LOX_IR_DATA_NODE_CONSTANT)
        && (GET_CONST_VALUE_LOX_IR_NODE(binary->left) == 1 || GET_CONST_VALUE_LOX_IR_NODE(binary->right) == 1);
}

static bool is_binary_dec(struct lox_ir_data_binary_node * binary) {
    return binary->operator == OP_SUB
           && binary->data.produced_type->type == LOX_IR_TYPE_NATIVE_I64
           && (binary->left->type == LOX_IR_DATA_NODE_CONSTANT || binary->right->type == LOX_IR_DATA_NODE_CONSTANT)
           && (GET_CONST_VALUE_LOX_IR_NODE(binary->left) == 1 || GET_CONST_VALUE_LOX_IR_NODE(binary->right) == 1);
}

static struct lox_ir_ll_operand emit_operand_to_register(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand operand,
        bool is_fp
) {
    switch (operand.type) {
        case LOX_IR_LL_OPERAND_REGISTER: return operand;
        case LOX_IR_LL_OPERAND_MEMORY_ADDRESS:
        case LOX_IR_LL_OPERAND_STACK_SLOT:
        case LOX_IR_LL_OPERAND_IMMEDIATE: {
            struct lox_ir_ll_operand v_register = alloc_new_v_register(lllil, is_fp);
            emit_move_ll_lox_ir(lllil, v_register, operand);
            return v_register;
        }
        case LOX_IR_LL_OPERAND_FLAGS: {
            struct lox_ir_ll_operand v_register = alloc_new_v_register(lllil, false);
            emit_unary_ll_lox_ir(lllil, operand, UNARY_LL_LOX_IR_FLAGS_TO_NATIVE_BOOL);
            return v_register;
        }
        default: //TODO Runtiem panic
    }
}

static void emit_lox_to_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand to_cast,
        lox_ir_type_t actual_type, //Has lox format
        lox_ir_type_t desired_type //Has native format
) {
    if (actual_type == LOX_IR_TYPE_F64 && desired_type == LOX_IR_TYPE_NATIVE_I64) {
        emit_unary_ll_lox_ir(lllil, to_cast, UNARY_LL_LOX_IR_F64_TO_I64_CAST);
        return;
    } else if (actual_type == LOX_IR_TYPE_LOX_I64 && desired_type == LOX_IR_TYPE_NATIVE_I64) {
        emit_unary_ll_lox_ir(lllil, to_cast, UNARY_LL_LOX_IR_F64_TO_I64_CAST);
        return;
    } else if (actual_type == LOX_IR_TYPE_LOX_I64 && desired_type == LOX_IR_TYPE_F64) {
        return;
    }

    if(!is_format_equivalent_lox_ir_type(actual_type, desired_type)){
        //TODO Runtime panic
    }

    switch (actual_type) {
        case LOX_IR_TYPE_LOX_ANY: emit_lox_any_to_native_cast(lllil, to_cast); break;
        case LOX_IR_TYPE_LOX_I64: emit_lox_number_to_native_i64_cast(lllil, to_cast); break;
        case LOX_IR_TYPE_LOX_BOOLEAN: emit_lox_boolean_to_native(lllil, to_cast); break;
        case LOX_IR_TYPE_LOX_STRING: emit_lox_string_to_native(lllil, to_cast); break;
        case LOX_IR_TYPE_LOX_ARRAY: emit_lox_array_to_native(lllil, to_cast); break;
        case LOX_IR_TYPE_LOX_STRUCT_INSTANCE: emit_lox_struct_instance_to_native(lllil, to_cast); break;
        default: //TODO Runtime panic
    }
}

static void emit_native_to_lox(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand to_cast,
        lox_ir_type_t actual_type,
        lox_ir_type_t desired_type
) {
    switch (actual_type) {
        case LOX_IR_TYPE_NATIVE_NIL: break; //TODO runtime panic
        case LOX_IR_TYPE_F64: {
            //TODO Extract to method
            if (actual_type == LOX_IR_TYPE_NATIVE_I64 && desired_type == LOX_IR_TYPE_LOX_I64) {
                emit_unary_ll_lox_ir(lllil, to_cast, UNARY_LL_LOX_IR_I64_TO_F64_CAST);
            }
            if (actual_type == LOX_IR_TYPE_F64 && desired_type == LOX_IR_TYPE_LOX_I64) {
                emit_unary_ll_lox_ir(lllil, to_cast, UNARY_LL_LOX_IR_F64_TO_I64_CAST);
            }
            break;
        }
        case LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE: emit_native_struct_instance_to_lox(lllil, to_cast); break;
        case LOX_IR_TYPE_NATIVE_BOOLEAN: emit_native_bool_to_lox_bool(lllil, to_cast); break;
        case LOX_IR_TYPE_NATIVE_I64: emit_native_i64_to_lox_number(lllil, to_cast); break;
        case LOX_IR_TYPE_NATIVE_STRING: emit_native_string_to_lox(lllil, to_cast); break;
        case LOX_IR_TYPE_NATIVE_ARRAY: emit_native_array_to_lox(lllil, to_cast); break;
    }
}

//Given: cast(a) + a Without this check, when lowering lox ir the cast(a) node will override the vregister "a" with
//the cast value of a so when we use "a" again, it will contain the casted value which we might not want, for example
//in this case in the right operand of the add
static bool parent_will_have_unwanted_side_effect(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand parent_input,
        struct lox_ir_data_node * parent
) {
    return parent != NULL
        && (parent->type == LOX_IR_DATA_NODE_CAST || parent->type == LOX_IR_DATA_NODE_UNARY)
        && parent_input.type == LOX_IR_LL_OPERAND_REGISTER
        && parent_input.v_register.number < lllil->lllil->last_phi_resolution_v_reg_allocated;
}

static struct lox_ir_ll_operand copy_into_new_register(struct lllil_control * lllil, struct lox_ir_ll_operand src) {
    if (src.type != LOX_IR_LL_OPERAND_REGISTER) {
        return src;
    }

    struct lox_ir_ll_operand dst = alloc_new_v_register(lllil, src.v_register.is_float_register);
    emit_move_ll_lox_ir(lllil, dst, src);
    return dst;
}