#include "ir_lowerer_data.h"

typedef struct lox_ir_ll_operand(* lowerer_lox_ir_data_t)(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_v_register(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_global(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_constant(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_unary(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_unbox(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_box(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_struct(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_length(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_guard(struct lllil_control*, struct lox_ir_data_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_binary(struct lllil_control*, struct lox_ir_data_node*);

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
static struct lox_ir_ll_operand emit_lox_object_ptr_to_native(struct lllil_control *lllil, struct lox_ir_ll_operand input);
static struct lox_ir_ll_operand emit_object_ptr_to_lox_native(struct lllil_control *lllil, struct lox_ir_ll_operand input);
static struct lox_ir_ll_operand emit_native_i64_to_lox_number(struct lllil_control *lllil, struct lox_ir_ll_operand input);
static struct lox_ir_ll_operand emit_native_bool_to_lox_bool(struct lllil_control *lllil, struct lox_ir_ll_operand input);
static struct lox_ir_ll_operand emit_native_struct_instance_to_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_native_array_to_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_native_struct_instance_to_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_native_string_to_lox(struct lllil_control*, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field_escapes(struct lllil_control*, struct lox_ir_data_get_struct_field_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field_doest_not_escape(struct lllil_control*, struct lox_ir_data_get_struct_field_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_struct_escapes(struct lllil_control*, struct lox_ir_data_initialize_struct_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_struct_does_not_escape(struct lllil_control*, struct lox_ir_data_initialize_struct_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array_escapes(struct lllil_control*, struct lox_ir_data_initialize_array_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array_does_not_escape(struct lllil_control*, struct lox_ir_data_initialize_array_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element_does_not_escape(struct lllil_control*, struct lox_ir_data_get_array_element_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element_escapes(struct lllil_control*, struct lox_ir_data_get_array_element_node*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_length_does_not_escape(struct lllil_control*, struct lox_ir_data_get_array_length*);
static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_length_escapes(struct lllil_control*, struct lox_ir_data_get_array_length*);
static struct lox_ir_ll_operand emit_binary_sub(struct lllil_control*, lox_ir_type_t, struct lox_ir_ll_operand, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_binary_add(struct lllil_control*, lox_ir_type_t, struct lox_ir_ll_operand, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_binary_div(struct lllil_control*, lox_ir_type_t, struct lox_ir_ll_operand, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_binary_mul(struct lllil_control*, lox_ir_type_t, struct lox_ir_ll_operand, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_binary_and(struct lllil_control*, struct lox_ir_ll_operand, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_binary_op(struct lllil_control*, binary_operator_type_ll_lox_ir, struct lox_ir_ll_operand, struct lox_ir_ll_operand);
static struct lox_ir_ll_operand emit_lox_generic_binary(struct lllil_control*, bytecode_t, struct lox_ir_ll_operand, struct lox_ir_ll_operand);

static bool is_struct_field_fp(struct lox_ir_data_node *, char * field_name);
static lox_ir_type_t get_required_binary_operand_types(struct lox_ir_data_binary_node *binary_node);

extern struct struct_instance_object * alloc_struct_instance_gc_alg(struct struct_definition_object*);
extern void runtime_panic(char * format, ...);
extern uint64_t any_to_native_cast_jit_runime(lox_value_t);
extern struct array_object * alloc_array_gc_alg(int);
lox_value_t addition_lox(lox_value_t a, lox_value_t b);

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
        [LOX_IR_DATA_NODE_UNBOX] = lowerer_lox_ir_data_unbox,
        [LOX_IR_DATA_NODE_BOX] = lowerer_lox_ir_data_box,
        
        //I will do this when I have a copiler which works on functions
        [LOX_IR_DATA_NODE_CALL] = NULL,

        [LOX_IR_DATA_NODE_GET_SSA_NAME] = NULL,
        [LOX_IR_DATA_NODE_GET_LOCAL] = NULL,
        [LOX_IR_DATA_NODE_PHI] = NULL,
};

//If expected type is:
// - LOX_IR_TYPE_LOX_ANY the result type will have lox byte format
// - LOX_IR_TYPE_UNKNOWN the result type will have any type
struct lox_ir_ll_operand lower_lox_ir_data(
        struct lllil_control * lllil_control,
        struct lox_ir_data_node * data_node,
        lox_ir_type_t expected_type
) {
    lowerer_lox_ir_data_t lowerer_lox_ir_data = lowerer_lox_ir_by_data_node[data_node->type];

    if (lowerer_lox_ir_data == NULL) {
        runtime_panic("Unexpected data node %i in ir_lowerer_data", data_node->type);
    }

    struct lox_ir_ll_operand operand_result = lowerer_lox_ir_data(lllil_control, data_node);

    return operand_result;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_binary(
        struct lllil_control * control,
        struct lox_ir_data_node * data_node
) {
    struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) data_node;
    lox_ir_type_t binary_produced_type = binary->data.produced_type->type;
    lox_ir_type_t operands_required_type =  get_required_binary_operand_types(binary);
    struct lox_ir_ll_operand right = lower_lox_ir_data(control, binary->right, operands_required_type);
    struct lox_ir_ll_operand left = lower_lox_ir_data(control, binary->left, operands_required_type);

    if (binary_produced_type == LOX_IR_TYPE_LOX_ANY) {
        return emit_lox_generic_binary(control, binary->operator, left, right);
    }

    switch (binary->operator) {
        case OP_ADD:
            return emit_binary_add(control, binary_produced_type, left, right);
        case OP_SUB:
            return emit_binary_sub(control, binary_produced_type, left, right);
        case OP_MUL:
            return emit_binary_mul(control, binary_produced_type, left, right);
        case OP_DIV:
            return emit_binary_div(control, binary_produced_type, left, right);
        case OP_MODULO:
            return emit_binary_op(control, BINARY_LL_LOX_IR_MOD, left, right);
        case OP_BINARY_OP_AND:
            return emit_binary_op(control, BINARY_LL_LOX_IR_AND, left, right);
        case OP_BINARY_OP_OR:
            return emit_binary_op(control, BINARY_LL_LOX_IR_OR, left, right);
        case OP_LEFT_SHIFT:
            return emit_binary_op(control, BINARY_LL_LOX_IR_LEFT_SHIFT, left, right);
        case OP_RIGHT_SHIFT:
            return emit_binary_op(control, BINARY_LL_LOX_IR_RIGHT_SHIFT, left, right);
        case OP_GREATER:
            return emit_binary_op(control, BINARY_LL_LOX_IR_LEFT_SHIFT, left, right);
        case OP_LESS:
            return emit_binary_op(control, BINARY_LL_LOX_IR_LEFT_SHIFT, left, right);
        case OP_EQUAL:
            return emit_binary_op(control, BINARY_LL_LOX_IR_LEFT_SHIFT, left, right);
        default:
            //TODO Panic
    }
}

static lox_ir_type_t get_required_binary_operand_types(struct lox_ir_data_binary_node * binary_node) {
    if(binary_node->data.produced_type->type == LOX_IR_TYPE_LOX_ANY){
        return LOX_IR_TYPE_LOX_ANY;
    }

    switch (binary_node->operator) {
        case OP_ADD: {
            if (IS_STRING_LOX_IR_TYPE(binary_node->data.produced_type->type)) {
                return LOX_IR_TYPE_UNKNOWN;
            }
        }
        case OP_GREATER:
        case OP_LESS:
        case OP_MUL:
        case OP_DIV:
        case OP_SUB:
        case OP_EQUAL: {
            struct lox_ir_type * right_type = binary_node->right->produced_type;
            struct lox_ir_type * left_type = binary_node->left->produced_type;

            if (is_native_lox_ir_type(left_type->type) || left_type->type == LOX_IR_TYPE_F64) {
                return left_type->type;
            }
            if (is_native_lox_ir_type(right_type->type) || right_type->type == LOX_IR_TYPE_F64) {
                return right_type->type;
            }

            //At this point left & right types have the lox binar format
            return lox_type_to_native_lox_ir_type(left_type->type);
        }
        case OP_BINARY_OP_AND:
        case OP_BINARY_OP_OR:
        case OP_LEFT_SHIFT:
        case OP_RIGHT_SHIFT:
        case OP_MODULO: {
            return LOX_IR_TYPE_NATIVE_I64;
        }
        default: {
            //TODO Panic
        }
    }
}

static struct lox_ir_ll_operand emit_lox_generic_binary(
        struct lllil_control * control,
        bytecode_t operator,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right
) {
    if(operator != OP_ADD){
        //TODO Panic
    }

    struct v_register return_value = alloc_v_register_lox_ir(control->lllil->lox_ir, false);

    emit_function_call_with_return_value_ll_lox_ir(
            control,
            addition_lox,
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
        struct lox_ir_ll_operand right
) {
    struct lox_ir_ll_operand result = alloc_new_v_register(control, false);
    emit_binary_ll_lox_ir(control,op,left,right);
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
            left,
            right
    );

    return result;
}

static struct lox_ir_ll_operand emit_binary_sub(
        struct lllil_control * control,
        lox_ir_type_t type_to_produce,
        struct lox_ir_ll_operand left_operand,
        struct lox_ir_ll_operand right_operand
) {
    bool is_fp = type_to_produce == LOX_IR_TYPE_F64;
    bool is_lox_i64 = type_to_produce == LOX_IR_TYPE_LOX_I64;

    struct lox_ir_ll_operand result = alloc_new_v_register(control, is_fp);

    if (is_fp) {
        emit_fsub_ll_lox_ir(control, result, left_operand, right_operand);
    } else {
        emit_isub_ll_lox_ir(control, result, left_operand, right_operand);
    }

    return result;
}

static struct lox_ir_ll_operand emit_binary_div(
        struct lllil_control * control,
        lox_ir_type_t type_to_produce,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right
) {
    bool is_fp = type_to_produce == LOX_IR_TYPE_F64;
    struct lox_ir_ll_operand result = alloc_new_v_register(control, is_fp);

    if (is_fp) {
        emit_fdiv_ll_lox_ir(control, result, left, right);
    } else {
        emit_idiv_ll_lox_ir(control, result, left, right);
    }

    return result;
}

static struct lox_ir_ll_operand emit_binary_mul(
        struct lllil_control * control,
        lox_ir_type_t type_to_produce,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right
) {
    bool is_fp = type_to_produce == LOX_IR_TYPE_F64;
    struct lox_ir_ll_operand result = alloc_new_v_register(control, is_fp);

    if (is_fp) {
        emit_fmul_ll_lox_ir(control, result, left, right);
    } else {
        emit_fmul_ll_lox_ir(control, result, left, right);
    }

    return result;
}

static struct lox_ir_ll_operand emit_binary_add(
        struct lllil_control * control,
        lox_ir_type_t type_to_produce,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right
) {
    bool is_string = type_to_produce == IS_STRING_LOX_IR_TYPE(type_to_produce);
    bool is_fp = type_to_produce == LOX_IR_TYPE_F64;

    struct lox_ir_ll_operand result = alloc_new_v_register(control, is_fp);

    if (is_string){
        emit_string_concat_ll_lox_ir(control, result, left, right);
    } else if (is_fp) {
        emit_fadd_ll_lox_ir(control, result, left, right);
    } else {
        emit_iadd_ll_lox_ir(control, result, left, right);
    }

    return result;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_guard(
        struct lllil_control * lllil_control,
        struct lox_ir_data_node * data_node
) {
    struct lox_ir_data_guard_node * guard_node = (struct lox_ir_data_guard_node *) data_node;

    return emit_guard_ll_lox_ir(
            lllil_control,
            guard_node->guard
    );
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_length(
        struct lllil_control * lllil_control,
        struct lox_ir_data_node * data_node
) {
    struct lox_ir_data_get_array_length * get_length = (struct lox_ir_data_get_array_length *) data_node;

    if (!get_length->escapes) {
        return lowerer_lox_ir_data_get_array_length_does_not_escape(lllil_control, get_length);
    } else {
        return lowerer_lox_ir_data_get_array_length_escapes(lllil_control, get_length);
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_length_escapes(
        struct lllil_control * control,
        struct lox_ir_data_get_array_length * get_arry_length
) {
    struct lox_ir_ll_operand array_length_address = lower_lox_ir_data(control, get_arry_length->instance,
            LOX_IR_TYPE_NATIVE_ARRAY);

    emit_isub_ll_lox_ir(
            control,
            array_length_address,
            array_length_address,
            IMMEDIATE_TO_OPERAND(sizeof(int))
    );

    struct lox_ir_ll_operand array_length_value = alloc_new_v_register(control, false);

    emit_load_at_offset_ll_lox_ir(
            control,
            array_length_value,
            array_length_address,
            0
    );

    return array_length_value;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_length_does_not_escape(
        struct lllil_control * control,
        struct lox_ir_data_get_array_length * get_arry_length
) {
    struct lox_ir_ll_operand array_length_address = lower_lox_ir_data(control, get_arry_length->instance,
            LOX_IR_TYPE_NATIVE_ARRAY);

    emit_iadd_ll_lox_ir(
            control,
            array_length_address,
            array_length_address,
            IMMEDIATE_TO_OPERAND(offsetof(struct lox_arraylist, in_use))
    );

    struct lox_ir_ll_operand array_length_value = alloc_new_v_register(control, false);

    emit_load_at_offset_ll_lox_ir(
            control,
            array_length_value,
            array_length_address,
            0
    );

    return array_length_value;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element(
        struct lllil_control * lllil_control,
        struct lox_ir_data_node * data_node
) {
    struct lox_ir_data_get_array_element_node * get_arr_element = (struct lox_ir_data_get_array_element_node *) data_node;

    if (!get_arr_element->escapes) {
        lowerer_lox_ir_data_get_array_element_does_not_escape(lllil_control, get_arr_element);
    } else {
        lowerer_lox_ir_data_get_array_element_escapes(lllil_control, get_arr_element);
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element_does_not_escape(
        struct lllil_control * lllil,
        struct lox_ir_data_get_array_element_node * node
) {
    struct lox_ir_ll_operand instance = lower_lox_ir_data(lllil, node->index, LOX_IR_TYPE_NATIVE_ARRAY);
    struct lox_ir_ll_operand index = lower_lox_ir_data(lllil, node->index, LOX_IR_TYPE_NATIVE_I64);

    if(node->requires_range_check){
        emit_range_check_ll_lox_ir(lllil, instance, index);
    }

    struct lox_ir_ll_operand array_element = alloc_new_v_register(lllil, false);

    return array_element;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_array_element_escapes(
        struct lllil_control * lllil,
        struct lox_ir_data_get_array_element_node * node
) {
    struct lox_ir_ll_operand instance = lower_lox_ir_data(lllil, node->index, LOX_IR_TYPE_NATIVE_ARRAY);
    struct lox_ir_ll_operand index = lower_lox_ir_data(lllil, node->index, LOX_IR_TYPE_NATIVE_I64);

    if(node->requires_range_check){
        emit_range_check_ll_lox_ir(lllil, instance, index);
    }

    struct lox_ir_ll_operand array_element = alloc_new_v_register(lllil, false);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array(
        struct lllil_control * lllil_control,
        struct lox_ir_data_node * node
) {
    struct lox_ir_data_initialize_array_node * init_arr = (struct lox_ir_data_initialize_array_node *) node;
    size_t array_size = init_arr->n_elements * sizeof(uint64_t);

    if (!init_arr->escapes && array_size < 512) {
        return lowerer_lox_ir_data_initialize_array_does_not_escape(lllil_control, init_arr);
    } else {
        return lowerer_lox_ir_data_initialize_array_escapes(lllil_control, init_arr);
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_initialize_array_does_not_escape(
        struct lllil_control * lllil,
        struct lox_ir_data_initialize_array_node * init_array
) {
    size_t array_size = sizeof(int) + sizeof(uint64_t) * init_array->n_elements;
    int stack_slot = allocate_stack_slot_lllil(lllil->lllil, lllil->control_node->block, array_size);

    //Move array size
    emit_move_ll_lox_ir(
            lllil,
            STACKSLOT_TO_OPERAND(stack_slot, 0),
            IMMEDIATE_TO_OPERAND(array_size)
    );

    for (int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++) {
        struct lox_ir_ll_operand array_element_reg = lower_lox_ir_data(lllil, init_array->elememnts[i],
                LOX_IR_TYPE_UNKNOWN);
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
        struct lox_ir_data_initialize_array_node * init_array
) {
    struct lox_ir_ll_operand array_instance_operand = alloc_new_v_register(lllil_control, false);
    int n_elements = init_array->n_elements;

    emit_function_call_with_return_value_ll_lox_ir(
            lllil_control,
            alloc_array_gc_alg,
            array_instance_operand.v_register,
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) init_array->n_elements)
    );

    emit_iadd_ll_lox_ir(
            lllil_control,
            array_instance_operand,
            array_instance_operand,
            IMMEDIATE_TO_OPERAND(sizeof(struct lox_arraylist))
    );

    for(int i = 0; i < n_elements && !init_array->empty_initialization; i++) {
        struct lox_ir_ll_operand struct_element_reg = lower_lox_ir_data(lllil_control, init_array->elememnts[i],
                LOX_IR_TYPE_UNKNOWN);

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
        struct lox_ir_data_node * data_node
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
            struct_instance_operand.v_register,
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) init_node->definition)
    );

    for (int i = 0; i < init_node->definition->n_fields; i++) {
        struct string_object * field_name = definition->field_names[definition->n_fields - i - 1];

        struct lox_ir_ll_operand struct_element_reg = lower_lox_ir_data(lllil, init_node->fields_nodes[i], LOX_IR_TYPE_UNKNOWN);

        emit_function_call_ll_lox_ir(
                lllil,
                put_hash_table,
                3,
                struct_instance_operand,
                field_name,
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
    int stack_slot = allocate_stack_slot_lllil(lllil->lllil, lllil->control_node->block, struct_size);

    for (int i = 0; i < definition->n_fields; i++) {
        struct string_object * field_name = definition->field_names[definition->n_fields - i - 1];
        struct lox_ir_ll_operand struct_element_reg = lower_lox_ir_data(lllil, init_node->fields_nodes[i], LOX_IR_TYPE_UNKNOWN);
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
        struct lox_ir_data_node * data_node
) {
    struct lox_ir_data_get_struct_field_node  * get_struct_field = (struct lox_ir_data_get_struct_field_node *) data_node;

    if(!get_struct_field->escapes) {
        return lowerer_lox_ir_data_get_struct_field_doest_not_escape(lllil, get_struct_field);
    } else {
        return lowerer_lox_ir_data_get_struct_field_escapes(lllil, get_struct_field);
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field_doest_not_escape(
        struct lllil_control * lllil,
        struct lox_ir_data_get_struct_field_node * get_struct_field
) {
    struct lox_ir_ll_operand struct_instance = lower_lox_ir_data(lllil, get_struct_field->instance,
            LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE);

    bool is_field_fp = is_struct_field_fp(get_struct_field->instance, get_struct_field->field_name->chars);
    struct lox_ir_ll_operand get_struct_field_reg_result = V_REG_TO_OPERAND(
            alloc_v_register_lox_ir(lllil->lllil->lox_ir, is_field_fp));

    emit_function_call_with_return_value_ll_lox_ir(
            lllil,
            get_hash_table,
            get_struct_field_reg_result.v_register,
            2,
            struct_instance,
            IMMEDIATE_TO_OPERAND((uint64_t) get_struct_field->field_name->chars)
    );

    return get_struct_field_reg_result;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_struct_field_escapes(
        struct lllil_control * lllil,
        struct lox_ir_data_get_struct_field_node * get_struct_field
) {
    //If it escapes, it should produce a native pointer to struct instance
    struct lox_ir_ll_operand struct_instance = lower_lox_ir_data(lllil, get_struct_field->instance,
            LOX_IR_TYPE_UNKNOWN);

    //TODO Guarantee that get_struct_field->instance produces a definition type
    struct struct_definition_object * definition = get_struct_field->instance->produced_type->value.struct_instance->definition;
    bool is_field_fp = is_struct_field_fp(get_struct_field->instance, get_struct_field->field_name->chars);

    int field_offset = get_offset_field_struct_definition_ll_lox_ir(definition, get_struct_field->field_name->chars);

    struct lox_ir_ll_operand get_struct_field_reg_result = V_REG_TO_OPERAND(alloc_v_register_lox_ir(
            lllil->lllil->lox_ir, is_field_fp));

    emit_load_at_offset_ll_lox_ir(
            lllil,
            get_struct_field_reg_result,
            struct_instance,
            field_offset
    );

    return get_struct_field_reg_result;
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_box(
        struct lllil_control * lllil,
        struct lox_ir_data_node * data_node
) {
    struct lox_ir_data_box_node * box = (struct lox_ir_data_box_node *) data_node;
    struct lox_ir_ll_operand to_unbox_input = lower_lox_ir_data(lllil, box->to_box, LOX_IR_TYPE_UNKNOWN);

    switch (box->to_box->produced_type->type) {
        case LOX_IR_TYPE_NATIVE_NIL: {
            struct lox_ir_ll_operand nil_value_reg = V_REG_TO_OPERAND(alloc_v_register_lox_ir(lllil->lllil->lox_ir, false));
            emit_move_ll_lox_ir(lllil, nil_value_reg, IMMEDIATE_TO_OPERAND((uint64_t) NIL_VALUE));
            return nil_value_reg;
        }
        case LOX_IR_TYPE_F64: {
            return to_unbox_input;
        }
        case LOX_IR_TYPE_NATIVE_I64: {
            return emit_native_i64_to_lox_number(lllil, to_unbox_input);
        }
        case LOX_IR_TYPE_NATIVE_BOOLEAN: {
            return emit_native_bool_to_lox_bool(lllil, to_unbox_input);
        }
        case LOX_IR_TYPE_NATIVE_STRING: {
            return emit_native_string_to_lox(lllil, to_unbox_input);
        }
        case LOX_IR_TYPE_NATIVE_ARRAY: {
            return emit_native_array_to_lox(lllil, to_unbox_input);
        }
        case LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE: {
            return emit_native_struct_instance_to_lox(lllil, to_unbox_input);
        }
    }
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_unbox(
        struct lllil_control * lllil,
        struct lox_ir_data_node * data_node
) {
    //TODO Make the guarantee to be in a vregister not an immediate value,
    struct lox_ir_data_unbox_node * unbox = (struct lox_ir_data_unbox_node *) data_node;
    struct lox_ir_ll_operand to_unbox_input = lower_lox_ir_data(lllil, unbox->to_unbox, LOX_IR_TYPE_UNKNOWN);

    switch (unbox->to_unbox->produced_type->type) {
        case LOX_IR_TYPE_F64: {
            return to_unbox_input;
        }
        case LOX_IR_TYPE_LOX_NIL: {
            struct lox_ir_ll_operand nil_value_reg = V_REG_TO_OPERAND(alloc_v_register_lox_ir(lllil->lllil->lox_ir, false));
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
        struct lllil_control * lllil,
        struct lox_ir_data_node * data_node
) {
    struct lox_ir_data_get_global_node * get_global = (struct lox_ir_data_get_global_node *) data_node;
    struct v_register global_v_register = alloc_v_register_lox_ir(lllil->lllil->lox_ir, false);

    emit_function_call_with_return_value_ll_lox_ir(
            lllil,
            &get_hash_table,
            global_v_register,
            2,
            IMMEDIATE_TO_OPERAND((uint64_t) &get_global->package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) &get_global->name)
    );

    return V_REG_TO_OPERAND(global_v_register);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_get_v_register(
        struct lllil_control * lllil,
        struct lox_ir_data_node * node
) {
    struct lox_ir_data_get_v_register_node * get_v_reg = (struct lox_ir_data_get_v_register_node *) node;
    return V_REG_TO_OPERAND(get_v_reg->v_register);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_constant(
        struct lllil_control * lllil,
        struct lox_ir_data_node * node
) {
    struct lox_ir_data_constant_node * const_node = (struct lox_ir_data_constant_node *) node;
    return IMMEDIATE_TO_OPERAND(const_node->value);
}

static struct lox_ir_ll_operand lowerer_lox_ir_data_unary(
        struct lllil_control * lllil,
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
        struct lllil_control * lllil,
        struct lox_ir_ll_operand unary_operand //Expect to be in register
) {
    struct lox_ir_ll_operand true_value_reg = alloc_new_v_register(lllil, false);
    struct lox_ir_ll_operand false_value_reg = alloc_new_v_register(lllil, false);

    emit_move_ll_lox_ir(lllil, true_value_reg, IMMEDIATE_TO_OPERAND(TRUE_VALUE));

    emit_isub_ll_lox_ir(lllil, unary_operand, true_value_reg, unary_operand);

    emit_move_ll_lox_ir(lllil, false_value_reg, IMMEDIATE_TO_OPERAND(FALSE_VALUE));

    emit_iadd_ll_lox_ir(lllil, unary_operand, unary_operand, false_value_reg);

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
    struct v_register return_value_vreg = alloc_v_register_lox_ir(lllil->lllil->lox_ir, false);

    emit_function_call_with_return_value_ll_lox_ir(
            lllil,
            &any_to_native_cast_jit_runime,
            return_value_vreg,
            1,
            input
    );

    return V_REG_TO_OPERAND(return_value_vreg);
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
    emit_isub_ll_lox_ir(
            lllil,
            input,
            input,
            IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields))
    );

    return emit_lox_object_ptr_to_native(lllil, input);
}

static struct lox_ir_ll_operand emit_lox_struct_instance_to_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native(lllil, input);

    emit_iadd_ll_lox_ir(
            lllil,
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
    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native(lllil, input);

    emit_iadd_ll_lox_ir(
            lllil,
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
    emit_isub_ll_lox_ir(
            lllil,
            input,
            input,
            IMMEDIATE_TO_OPERAND(offsetof(struct array_object, values) + offsetof(struct lox_arraylist, in_use))
    );

    return emit_lox_object_ptr_to_native(lllil, input);
}

static struct lox_ir_ll_operand emit_native_string_to_lox(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    uint64_t offset = offsetof(struct array_object, values) + offsetof(struct lox_arraylist, in_use);

    emit_isub_ll_lox_ir(
            lllil,
            input,
            input,
            IMMEDIATE_TO_OPERAND(offset)
    );

    return emit_lox_object_ptr_to_native(lllil, input);
}

static struct lox_ir_ll_operand emit_lox_array_to_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input
) {
    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native(lllil, input);
    uint64_t offset = offsetof(struct array_object, values) + offsetof(struct lox_arraylist, in_use);

    emit_iadd_ll_lox_ir(
            lllil,
            native_object_ptr,
            native_object_ptr,
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
    emit_iadd_ll_lox_ir(
            lllil,
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
    emit_isub_ll_lox_ir(lllil, input, input, IMMEDIATE_TO_OPERAND(TRUE_VALUE));
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
            IMMEDIATE_TO_OPERAND(FLOAT_SIGN_BIT | FLOAT_QNAN)
    );

    return input;
}

static struct lox_ir_ll_operand emit_lox_object_ptr_to_native(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input //Expect v register
) {
    struct v_register or_fsb_qfn_reg = alloc_v_register_lox_ir(lllil->lllil->lox_ir, false);

    emit_move_ll_lox_ir(
            lllil,
            V_REG_TO_OPERAND(or_fsb_qfn_reg),
            IMMEDIATE_TO_OPERAND(~(FLOAT_SIGN_BIT | FLOAT_QNAN))
    );

    //See AS_OBJECT macro in types.h
    emit_binary_ll_lox_ir(
            lllil,
            BINARY_LL_LOX_IR_AND,
            input,
            IMMEDIATE_TO_OPERAND(~(FLOAT_SIGN_BIT | FLOAT_QNAN))
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
    struct struct_definition_object * definition = struct_instance_type->definition;

    if (definition == NULL) {
        return false;
    }

    struct lox_ir_type * field_value_type = get_u64_hash_table(&struct_instance_type->type_by_field_name, (uint64_t) field_name);
    return field_value_type != NULL && field_value_type->type == LOX_IR_TYPE_F64;;
}