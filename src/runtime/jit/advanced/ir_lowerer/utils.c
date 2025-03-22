#include "utils.h"

static void emit_guard_boolean_check(struct lllil_control*,struct lox_ir_ll_operand,struct lox_ir_guard);
static void emit_guard_array_type_check(struct lllil_control*,struct lox_ir_ll_operand,struct lox_ir_guard);
static void emit_guard_struct_definition_type_check(struct lllil_control*,struct lox_ir_ll_operand,struct lox_ir_guard);
static void emit_guard_type_check(struct lllil_control*,struct lox_ir_ll_operand,struct lox_ir_guard);
static void emit_guard_f64_type_check(struct lllil_control*,struct lox_ir_ll_operand);
static void emit_guard_i64_type_check(struct lllil_control*,struct lox_ir_ll_operand);
static void emit_guard_bool_type_check(struct lllil_control*,struct lox_ir_ll_operand);
static void emit_guard_nil_type_check(struct lllil_control*,struct lox_ir_ll_operand);
static void emit_guard_object_type_check(struct lllil_control*,struct lox_ir_ll_operand);

extern void guard_array_type_check_jit_runtime(lox_value_t);
extern void switch_to_interpreter_jit_runime();
extern void range_check_panic_jit_runime();


int get_offset_field_struct_definition_ll_lox_ir(
        struct struct_definition_object * definition,
        char * field_name
) {
    uint16_t current_offset = 0;

    for (int i = 0; i < definition->n_fields; i++, current_offset += 8) {
        struct string_object * current_field_name = definition->field_names[i];

        if (strcmp(current_field_name->chars, field_name) == 0) {
            return current_offset;
        }
    }

    return -1;
}

struct lox_ir_ll_operand emit_lox_object_ptr_to_native_ll_lox_ir(
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
            input,
            V_REG_TO_OPERAND(or_fsb_qfn_reg)
    );

    return input;
}

//move base + offset, value
void emit_store_at_offset_ll_lox_ir(
        struct lllil_control * lllil_control,
        struct lox_ir_ll_operand base,
        int offset,
        struct lox_ir_ll_operand value
) {
    emit_move_ll_lox_ir(lllil_control, ADDRESS_TO_OPERAND(base.v_register, offset), value);
}

//move value, base + offset
void emit_load_at_offset_ll_lox_ir(
        struct lllil_control * lllil_control,
        struct lox_ir_ll_operand value,
        struct lox_ir_ll_operand base,
        int offset
) {
    emit_move_ll_lox_ir(lllil_control, value, ADDRESS_TO_OPERAND(base.v_register, offset));
}

void emit_move_ll_lox_ir(
        struct lllil_control * control,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
) {
    struct lox_ir_control_ll_move * move = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_MOVE,struct lox_ir_control_ll_move,
            control->control_node_to_lower->block, LOX_IR_ALLOCATOR(control->lllil->lox_ir));
    move->from = b;
    move->to = a;

    add_lowered_node_lllil_control(control, &move->control);
}

struct lox_ir_ll_operand emit_guard_ll_lox_ir(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand guard_input,
        struct lox_ir_guard guard
) {
    switch (guard.type) {
        case LOX_IR_GUARD_BOOLEAN_CHECK: {
            emit_guard_boolean_check(lllil, guard_input, guard);
            break;
        }
        case LOX_IR_GUARD_TYPE_CHECK: {
            emit_guard_type_check(lllil, guard_input, guard);
            break;
        }
        case LOX_IR_GUARD_ARRAY_TYPE_CHECK: {
            emit_guard_array_type_check(lllil, guard_input, guard);
            break;
        }
        case LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK: {
            emit_guard_struct_definition_type_check(lllil, guard_input, guard);
            break;
        };
    }

    return guard_input;
}

static void emit_guard_struct_definition_type_check(
        struct lllil_control * control,
        struct lox_ir_ll_operand guard_input,
        struct lox_ir_guard guard
) {
    //After this we are guaranteed that guard input is a lox object type
    emit_guard_object_type_check(control, guard_input);

    if (guard.value_to_compare.struct_definition == NULL) {
        return;
    }

    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native_ll_lox_ir(control, guard_input);

    struct v_register object_type_v_reg = alloc_v_register_lox_ir(control->lllil->lox_ir, false);
    object_type_v_reg.register_bit_size = 32;

    //Check that the object type is a struct instance
    emit_move_ll_lox_ir(
            control,
            V_REG_TO_OPERAND(object_type_v_reg),
            ADDRESS_TO_OPERAND(native_object_ptr.v_register, offsetof(struct object, type))
    );
    emit_comparation_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_EQ,
            V_REG_TO_OPERAND(object_type_v_reg),
            IMMEDIATE_TO_OPERAND(OBJ_STRUCT_INSTANCE)
    );
    emit_conditional_function_call_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_NOT_EQ,
            &switch_to_interpreter_jit_runime,
            "switch_to_interpreter_jit_runime",
            0
    );

    //Check that the struct instance has the expected struct definition
    struct v_register actual_struct_definition_v_reg = alloc_v_register_lox_ir(control->lllil->lox_ir, false);
    struct struct_definition_object * expected_struct_definition = guard.value_to_compare.struct_definition;
    emit_move_ll_lox_ir(
            control,
            V_REG_TO_OPERAND(object_type_v_reg),
            ADDRESS_TO_OPERAND(native_object_ptr.v_register, offsetof(struct struct_instance_object, definition))
    );
    emit_comparation_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_EQ,
            V_REG_TO_OPERAND(object_type_v_reg),
            IMMEDIATE_TO_OPERAND((uint64_t) expected_struct_definition)
    );
    emit_conditional_function_call_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_NOT_EQ,
            &switch_to_interpreter_jit_runime,
            "switch_to_interpreter_jit_runime",
            0
    );
}

static void emit_guard_array_type_check(
        struct lllil_control * control,
        struct lox_ir_ll_operand guard_input,
        struct lox_ir_guard guard
) {
    //After this, we are guaranteed that guard input is a lox object type
    emit_guard_object_type_check(control, guard_input);

    emit_function_call_ll_lox_ir(
            control,
            &guard_array_type_check_jit_runtime,
            "guard_array_type_check_jit_runtime",
            0
    );
}

static void emit_guard_type_check(
        struct lllil_control * control,
        struct lox_ir_ll_operand guard_input,
        struct lox_ir_guard guard
) {
    switch (guard.value_to_compare.type) {
        case LOX_IR_TYPE_F64: {
            emit_guard_f64_type_check(control, guard_input);
            break;
        }
        case LOX_IR_TYPE_LOX_I64: {
            emit_guard_i64_type_check(control, guard_input);
            break;
        };
        case LOX_IR_TYPE_LOX_BOOLEAN: {
            emit_guard_bool_type_check(control, guard_input);
            break;
        };
        case LOX_IR_TYPE_LOX_NIL: {
            emit_guard_nil_type_check(control, guard_input);
            break;
        };
        //We are only going to check if the guard input is an object type, we are not going to check its content
        case LOX_IR_TYPE_LOX_ARRAY:
        case LOX_IR_TYPE_LOX_STRING:
        case LOX_IR_TYPE_LOX_STRUCT_INSTANCE: {
            emit_guard_object_type_check(control, guard_input);
            break;
        }
        case LOX_IR_TYPE_LOX_ANY: break;
        default: {
            //TODO Panic
        }
    }
}

//Implementation is the same as types.h's IS_OBJECT()
static void emit_guard_object_type_check(
        struct lllil_control * control,
        struct lox_ir_ll_operand guard_input
) {
    struct lox_ir_ll_operand result = V_REG_TO_OPERAND(alloc_v_register_lox_ir(control->lllil->lox_ir, false));

    emit_binary_ll_lox_ir(
            control,
            BINARY_LL_LOX_IR_AND,
            result,
            guard_input,
            IMMEDIATE_TO_OPERAND(FLOAT_QNAN | FLOAT_SIGN_BIT)
    );

    emit_comparation_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_EQ,
            result,
            IMMEDIATE_TO_OPERAND(FLOAT_QNAN | FLOAT_SIGN_BIT)
    );

    emit_conditional_function_call_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_NOT_EQ,
            &switch_to_interpreter_jit_runime,
            "switch_to_interpreter_jit_runime",
            0
    );
}

//Implementation is the same as types.h's IS_NIL()
static void emit_guard_nil_type_check(
        struct lllil_control * control,
        struct lox_ir_ll_operand guard_input
) {
    emit_comparation_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_EQ,
            guard_input,
            IMMEDIATE_TO_OPERAND(NIL_VALUE)
    );

    emit_conditional_function_call_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_NOT_EQ,
            &switch_to_interpreter_jit_runime,
            "switch_to_interpreter_jit_runime",
            0
    );
}

//Implementation is the same as types.h's IS_BOOL()
static void emit_guard_bool_type_check(
        struct lllil_control * control,
        struct lox_ir_ll_operand guard_input
) {
    struct lox_ir_ll_operand result = V_REG_TO_OPERAND(alloc_v_register_lox_ir(control->lllil->lox_ir, false));

    emit_binary_ll_lox_ir(
            control,
            BINARY_LL_LOX_IR_OR,
            result,
            guard_input,
            IMMEDIATE_TO_OPERAND(1)
    );

    emit_comparation_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_EQ,
            result,
            IMMEDIATE_TO_OPERAND(TRUE_VALUE)
    );

    emit_conditional_function_call_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_NOT_EQ,
            &switch_to_interpreter_jit_runime,
            "switch_to_interpreter_jit_runime",
            0
    );
}

static void emit_guard_i64_type_check(
        struct lllil_control * control,
        struct lox_ir_ll_operand guard_input
) {
    //After this we are guaranteed that guard_input has lox number format
    emit_guard_f64_type_check(control, guard_input);

    //Now we need to make sure that the number has no decimals.
    //To do that we will cast the number to integer, and we will cast it back to f64 and compare if there was a chnage.
    struct lox_ir_ll_operand a = V_REG_TO_OPERAND(alloc_v_register_lox_ir(control->lllil->lox_ir, false));
    struct lox_ir_ll_operand b = V_REG_TO_OPERAND(alloc_v_register_lox_ir(control->lllil->lox_ir, false));

    emit_move_ll_lox_ir(control, a, guard_input);
    emit_unary_ll_lox_ir(control, a, UNARY_LL_LOX_IR_F64_TO_I64_CAST);

    emit_move_ll_lox_ir(control, b, a);
    emit_unary_ll_lox_ir(control, b, UNARY_LL_LOX_IR_I64_TO_F64_CAST);

    emit_comparation_ll_lox_ir(control, COMPARATION_LL_LOX_IR_EQ, a, b);

    emit_conditional_function_call_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_NOT_EQ,
            &switch_to_interpreter_jit_runime,
            "switch_to_interpreter_jit_runime",
            0
    );
}

void emit_unary_ll_lox_ir(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand operand,
        unary_operator_type_ll_lox_ir operator
) {
    struct lox_ir_control_ll_unary * unary = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_UNARY,
            struct lox_ir_control_ll_unary, NULL, LOX_IR_ALLOCATOR(lllil->lllil->lox_ir));
    unary->operator = operator;
    unary->operand = operand;

    add_lowered_node_lllil_control(lllil, &unary->control);
}

void emit_binary_ll_lox_ir(
        struct lllil_control * lllil,
        binary_operator_type_ll_lox_ir binary_operator,
        struct lox_ir_ll_operand result,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right
) {
    struct lox_ir_control_ll_binary * binary = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_BINARY,
            struct lox_ir_control_ll_binary, NULL, LOX_IR_ALLOCATOR(lllil->lllil->lox_ir));
    binary->operator = binary_operator;
    binary->result = result;
    binary->right = right;
    binary->left = left;

    add_lowered_node_lllil_control(lllil, &binary->control);
}

void emit_comparation_ll_lox_ir(
        struct lllil_control * lllil,
        comparation_operator_type_ll_lox_ir comparation_operator,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right
) {
    struct lox_ir_control_ll_comparation * comparation = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_COMPARATION,
            struct lox_ir_control_ll_comparation, NULL, LOX_IR_ALLOCATOR(lllil->lllil->lox_ir));
    comparation->comparation_operator = comparation_operator;
    comparation->right = right;
    comparation->left = left;

    add_lowered_node_lllil_control(lllil, &comparation->control);
}

void emit_range_check_ll_lox_ir(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand instance,
        struct lox_ir_ll_operand index_to_access,
        bool array_instance_escapes
) {
    //Firts check: index_to_access < 0
    if (index_to_access.type != LOX_IR_LL_OPERAND_IMMEDIATE) {
        emit_comparation_ll_lox_ir(
                lllil,
                COMPARATION_LL_LOX_IR_LESS,
                index_to_access,
                IMMEDIATE_TO_OPERAND(0)
        );
        emit_conditional_function_call_ll_lox_ir(
                lllil,
                COMPARATION_LL_LOX_IR_IS_TRUE,
                &range_check_panic_jit_runime,
                "range_check_panic_jit_runime",
                0
        );
    }
    if (index_to_access.type == LOX_IR_LL_OPERAND_IMMEDIATE
        && index_to_access.immedate < 0) {
        //TODO Runtime panic
    }

    //Second check index_to_access >= array_size
    struct lox_ir_ll_operand array_size = emit_get_array_length_ll_lox_ir(lllil, instance,
            array_instance_escapes, alloc_v_register_lox_ir(lllil->lllil->lox_ir, false));
    emit_comparation_ll_lox_ir(
            lllil,
            COMPARATION_LL_LOX_IR_GREATER_EQ,
            array_size,
            index_to_access
    );
    emit_conditional_function_call_ll_lox_ir(
            lllil,
            COMPARATION_LL_LOX_IR_IS_TRUE,
            &range_check_panic_jit_runime,
            "range_check_panic_jit_runime",
            0
    );
}

//Implementation is the same as types.h's IS_NUMBER()
static void emit_guard_f64_type_check(
        struct lllil_control * control,
        struct lox_ir_ll_operand guard_input
) {
    struct v_register result = alloc_v_register_lox_ir(control->lllil->lox_ir, false);

    emit_binary_ll_lox_ir(
            control,
            BINARY_LL_LOX_IR_AND,
            V_REG_TO_OPERAND(result),
            guard_input,
            IMMEDIATE_TO_OPERAND(BINARY_LL_LOX_IR_AND)
    );

    emit_comparation_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_NOT_EQ,
            V_REG_TO_OPERAND(result),
            IMMEDIATE_TO_OPERAND(FLOAT_QNAN)
    );

    emit_conditional_function_call_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_IS_FALSE,
            &switch_to_interpreter_jit_runime,
            "switch_to_interpreter_jit_runime",
            0
    );
}

static void emit_guard_boolean_check(
        struct lllil_control * control,
        struct lox_ir_ll_operand guard_input,
        struct lox_ir_guard guard
) {
    uint64_t value_to_compare = guard.value_to_compare.check_true;

    emit_comparation_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_EQ,
            guard_input,
            IMMEDIATE_TO_OPERAND(value_to_compare)
    );

    emit_conditional_function_call_ll_lox_ir(
            control,
            COMPARATION_LL_LOX_IR_IS_FALSE,
            switch_to_interpreter_jit_runime,
            "switch_to_interpreter_jit_runime",
            0
    );
}

void emit_conditional_function_call_ll_lox_ir(
        struct lllil_control *lllil,
        comparation_operator_type_ll_lox_ir condition,
        void * function_address,
        char * function_name,
        int n_args,
        ... //Arguments
) {
    struct lox_allocator * allocator = &lllil->lllil->lox_ir->nodes_allocator_arena->lox_allocator;

    struct lox_ir_control_ll_cond_function_call * cond_func_call = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_LL_COND_FUNCTION_CALL, struct lox_ir_control_ll_cond_function_call, NULL, allocator
    );
    //Only used to store data, not as a "real" node in the IR
    struct lox_ir_control_ll_function_call * func_call = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL, struct lox_ir_control_ll_function_call, NULL, allocator
    );
    cond_func_call->call = func_call;
    cond_func_call->condition = condition;

    cond_func_call->call->function_name = function_name;
    cond_func_call->call = func_call;
    cond_func_call->call->function_call_address = function_address;
    init_ptr_arraylist(&cond_func_call->call->arguments, allocator);

    resize_ptr_arraylist(&cond_func_call->call->arguments, n_args);
    struct lox_ir_ll_operand arguments[n_args];
    VARARGS_TO_ARRAY(struct lox_ir_ll_operand, arguments, n_args, ...);
    for (int i = 0; i < n_args; i++) {
        struct lox_ir_ll_operand argument = arguments[i];
        struct lox_ir_ll_operand * argument_ptr = LOX_MALLOC(allocator, sizeof(argument));
        *argument_ptr = argument;
        append_ptr_arraylist(&cond_func_call->call->arguments, argument_ptr);
    }

    add_lowered_node_lllil_control(lllil, &cond_func_call->control);
}

void emit_function_call_with_return_value_ll_lox_ir(
        struct lllil_control *lllil,
        void * function_address,
        char * function_name,
        struct v_register return_value_v_reg,
        int n_args,
        ... //Arguments
) {
    struct lox_ir_ll_operand arguments[n_args];
    VARARGS_TO_ARRAY(struct lox_ir_ll_operand, arguments, n_args, ...);

    emit_function_call_with_return_value_manual_args_ll_lox_ir(lllil, function_address, function_name,
        return_value_v_reg, n_args, arguments);
}

void emit_function_call_with_return_value_manual_args_ll_lox_ir(
        struct lllil_control *lllil,
        void * function_address,
        char * function_name,
        struct v_register return_value_v_reg, //Return value register
        int n_args,
        struct lox_ir_ll_operand arguments[n_args]
) {
    struct lox_allocator * allocator = &lllil->lllil->lox_ir->nodes_allocator_arena->lox_allocator;

    struct lox_ir_control_ll_function_call * func_call = ALLOC_LOX_IR_CONTROL( //TODO
            LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL, struct lox_ir_control_ll_function_call, NULL, allocator
    );
    func_call->function_call_address = function_address;
    init_ptr_arraylist(&func_call->arguments, allocator);
    resize_ptr_arraylist(&func_call->arguments, n_args);
    func_call->has_return_value = true;
    func_call->function_name = function_name;
    func_call->return_value_v_reg = return_value_v_reg;

    for (int i = 0; i < n_args; i++) {
        struct lox_ir_ll_operand argument = arguments[i];
        struct lox_ir_ll_operand * argument_ptr = LOX_MALLOC(allocator, sizeof(argument));
        *argument_ptr = argument;

        append_ptr_arraylist(&func_call->arguments, argument_ptr);
    }

    add_lowered_node_lllil_control(lllil, &func_call->control);
}

void emit_function_call_ll_lox_ir(
        struct lllil_control * lllil,
        void * function_address,
        char * function_name,
        int n_args,
        ... //Arguments
) {
    struct lox_ir_ll_operand arguments[n_args];
    VARARGS_TO_ARRAY(struct lox_ir_ll_operand, arguments, n_args, ...);

    emit_function_call_manual_args_ll_lox_ir(lllil, function_address, function_name, n_args, arguments);
}

void emit_function_call_manual_args_ll_lox_ir(
        struct lllil_control *lllil,
        void * function_address,
        char * function_name,
        int n_args,
        struct lox_ir_ll_operand arguments[]
) {
    struct lox_allocator * allocator = &lllil->lllil->lox_ir->nodes_allocator_arena->lox_allocator;

    struct lox_ir_control_ll_function_call * func_call = ALLOC_LOX_IR_CONTROL( //TODO
            LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL, struct lox_ir_control_ll_function_call, NULL, allocator
    );
    func_call->function_call_address = function_address;
    func_call->function_name = function_name;
    init_ptr_arraylist(&func_call->arguments, allocator);
    resize_ptr_arraylist(&func_call->arguments, n_args);

    for (int i = 0; i < n_args; i++) {
        struct lox_ir_ll_operand argument = arguments[i];
        struct lox_ir_ll_operand * argument_ptr = LOX_MALLOC(allocator, sizeof(argument));
        *argument_ptr = argument;

        append_ptr_arraylist(&func_call->arguments, argument_ptr);
    }

    add_lowered_node_lllil_control(lllil, &func_call->control);
}

static struct lox_ir_ll_operand emit_get_array_length_ll_lox_ir_doesnt_escape(struct lllil_control*,
        struct lox_ir_ll_operand, struct v_register);
static struct lox_ir_ll_operand emit_get_array_length_ll_lox_ir_escapes(struct lllil_control*,
        struct lox_ir_ll_operand, struct v_register);

struct lox_ir_ll_operand emit_get_array_length_ll_lox_ir(
        struct lllil_control * control,
        struct lox_ir_ll_operand instance, //Expect type NATIVE_ARRAY_INSTANCE
        bool escapes,
        struct v_register result
) {
    if (!escapes) {
        return emit_get_array_length_ll_lox_ir_doesnt_escape(control, instance, result);
    } else {
        return emit_get_array_length_ll_lox_ir_escapes(control, instance, result);
    }
}

static struct lox_ir_ll_operand emit_get_array_length_ll_lox_ir_escapes(
        struct lllil_control * control,
        struct lox_ir_ll_operand instance, //Expect type NATIVE_ARRAY_INSTANCE
        struct v_register result
) {
    struct lox_ir_ll_operand array_length_value = V_REG_TO_OPERAND(result);

    emit_load_at_offset_ll_lox_ir(
            control,
            (array_length_value),
            instance,
            offsetof(struct lox_arraylist, in_use)
    );

    return array_length_value;
}

static struct lox_ir_ll_operand emit_get_array_length_ll_lox_ir_doesnt_escape(
        struct lllil_control * control,
        struct lox_ir_ll_operand instance, //Expect type NATIVE_ARRAY_INSTANCE
        struct v_register result
) {
    struct lox_ir_ll_operand array_length_value = V_REG_TO_OPERAND(result);

    emit_load_at_offset_ll_lox_ir(
            control,
            array_length_value,
            instance,
            - sizeof(int)
    );

    return array_length_value;
}
