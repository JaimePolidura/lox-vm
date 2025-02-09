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
            IMMEDIATE_TO_OPERAND(~(FLOAT_SIGN_BIT | FLOAT_QNAN))
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
    emit_move_ll_lox_ir(lllil_control, ADDRESS_TO_OPERAND(base, offset), value);
}

//move value, base + offset
void emit_load_at_offset_ll_lox_ir(
        struct lllil_control * lllil_control,
        struct lox_ir_ll_operand value,
        struct lox_ir_ll_operand base,
        int offset
) {
    emit_move_ll_lox_ir(lllil_control, value, ADDRESS_TO_OPERAND(base, offset));
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

    struct lox_ir_ll_operand native_object_ptr = emit_lox_object_ptr_to_native_ll_lox_ir(control, guard_input);

    struct v_register object_type_v_reg = alloc_v_register_lox_ir(control->lllil->lox_ir, false);
    object_type_v_reg.register_bit_size = 32;

    //Check that the object type is a struct instance
    emit_move_ll_lox_ir(
            control,
            V_REG_TO_OPERAND(object_type_v_reg),
            ADDRESS_TO_OPERAND(native_object_ptr, offsetof(struct object, type))
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
            0
    );

    //Check that the struct instance has the expected struct definition
    struct v_register actual_struct_definition_v_reg = alloc_v_register_lox_ir(control->lllil->lox_ir, false);
    struct struct_definition_object * expected_struct_definition = guard.value_to_compare.struct_definition;
    emit_move_ll_lox_ir(
            control,
            V_REG_TO_OPERAND(object_type_v_reg),
            ADDRESS_TO_OPERAND(native_object_ptr, offsetof(struct struct_instance_object, definition))
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
            0
    );
}

void emit_function_call_with_return_value_ll_lox_ir(
        struct lllil_control *lllil,
        void * function_address,
        struct v_register return_value_v_reg,
        int n_args,
        ... //Arguments
) {
    struct lox_allocator * allocator = &lllil->lllil->lox_ir->nodes_allocator_arena->lox_allocator;

    struct lox_ir_control_ll_function_call * func_call = ALLOC_LOX_IR_CONTROL( //TODO
            LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL, struct lox_ir_control_ll_function_call, NULL, allocator
    );
    func_call->function_call_address = function_address;
    init_ptr_arraylist(&func_call->arguments, allocator);
    resize_ptr_arraylist(&func_call->arguments, n_args);
    func_call->has_return_value = true;
    func_call->return_value_v_reg = return_value_v_reg;

    struct lox_ir_ll_operand arguments[n_args];
    VARARGS_TO_ARRAY(struct lox_ir_ll_operand, arguments, n_args, ...);

    for (int i = 0; i < n_args; i++) {
        struct lox_ir_ll_operand argument = arguments[i];
        struct lox_ir_ll_operand * argument_ptr = LOX_MALLOC(allocator, sizeof(argument));
        *argument_ptr = argument;

        append_ptr_arraylist(&func_call->arguments, argument_ptr);
    }

}

void emit_function_call_ll_lox_ir(
        struct lllil_control * lllil,
        void * function_address,
        int n_args,
        ... //Arguments
) {
    struct lox_allocator * allocator = &lllil->lllil->lox_ir->nodes_allocator_arena->lox_allocator;

    struct lox_ir_control_ll_function_call * func_call = ALLOC_LOX_IR_CONTROL( //TODO
            LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL, struct lox_ir_control_ll_function_call, NULL, allocator
    );
    func_call->function_call_address = function_address;
    init_ptr_arraylist(&func_call->arguments, allocator);
    resize_ptr_arraylist(&func_call->arguments, n_args);

    struct lox_ir_ll_operand arguments[n_args];
    VARARGS_TO_ARRAY(struct lox_ir_ll_operand, arguments, n_args, ...);

    for (int i = 0; i < n_args; i++) {
        struct lox_ir_ll_operand argument = arguments[i];
        struct lox_ir_ll_operand * argument_ptr = LOX_MALLOC(allocator, sizeof(argument));
        *argument_ptr = argument;

        append_ptr_arraylist(&func_call->arguments, argument_ptr);
    }
}