#include "ir_lowerer_control.h"

typedef void(* lowerer_lox_ir_control_t)(struct lllil_control*);
void lower_lox_ir_control_data(struct lllil_control*);
void lower_lox_ir_control_return(struct lllil_control*);
void lower_lox_ir_control_print(struct lllil_control*);
void lower_lox_ir_control_exit_monitor(struct lllil_control*);
void lower_lox_ir_control_enter_monitor(struct lllil_control*);
void lower_lox_ir_control_set_global(struct lllil_control*);
void lower_lox_ir_control_set_struct_field(struct lllil_control*);
void lower_lox_ir_control_set_array_element(struct lllil_control*);
void lower_lox_ir_control_loop(struct lllil_control*);
void lower_lox_ir_control_conditional_jump(struct lllil_control*);
void lower_lox_ir_control_guard(struct lllil_control*);
void lower_lox_ir_control_define_ssa_name(struct lllil_control*);

static bool is_redundant_register_move(struct lox_ir_ll_operand, struct lox_ir_control_define_ssa_name_node*);
static bool lowered_node_modifies_diferent_register_than_lowered_node_result(struct lllil_control*,struct lox_ir_ll_operand);

extern void enter_monitor(struct monitor * monitor);
extern void exit_monitor(struct monitor * monitor);
extern void set_self_thread_runnable();
extern void set_self_thread_waiting();
extern void print_lox_value(lox_value_t value);
extern void print_lox_value(lox_value_t value);
extern bool put_hash_table(struct lox_hash_table*, struct string_object*, lox_value_t);
extern void runtime_panic(char * format, ...);

lowerer_lox_ir_control_t lowerer_lox_ir_by_control_node[] = {
        [LOX_IR_CONTROL_NODE_DATA] = lower_lox_ir_control_data,
        [LOX_IR_CONTROL_NODE_RETURN] = lower_lox_ir_control_return,
        [LOX_IR_CONTROL_NODE_PRINT] = lower_lox_ir_control_print,
        [LOX_IR_CONTROL_NODE_ENTER_MONITOR] = lower_lox_ir_control_exit_monitor,
        [LOX_IR_CONTROL_NODE_EXIT_MONITOR] = lower_lox_ir_control_enter_monitor,
        [LOX_IR_CONTORL_NODE_SET_GLOBAL] = lower_lox_ir_control_set_global,
        [LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD] = lower_lox_ir_control_set_struct_field,
        [LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT] = lower_lox_ir_control_set_array_element,
        [LOX_IR_CONTROL_NODE_LOOP_JUMP] = lower_lox_ir_control_loop,
        [LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP] = lower_lox_ir_control_conditional_jump,
        [LOX_IR_CONTROL_NODE_GUARD] = lower_lox_ir_control_guard,
        [LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME] = lower_lox_ir_control_define_ssa_name,

        [LOX_IR_CONTORL_NODE_SET_LOCAL] = NULL,
        [LOX_IR_CONTROL_NODE_LL_MOVE] = NULL,
        [LOX_IR_CONTROL_NODE_LL_BINARY] = NULL,
        [LOX_IR_CONTROL_NODE_LL_UNARY] = NULL,
        [LOX_IR_CONTROL_NODE_LL_RETURN] = NULL,
        [LOX_IR_CONTROL_NODE_LL_FUNCTION_CALL] = NULL,
};

//Main function, entry point
void lower_lox_ir_control(struct lllil_control * lllil_control) {
    struct lox_ir_control_node * control_node = lllil_control->control_node_to_lower;
    lowerer_lox_ir_control_t lowerer_lox_ir_control = lowerer_lox_ir_by_control_node[control_node->type];
    if (lowerer_lox_ir_control == NULL) {
        runtime_panic("Unexpected control control %i in ir_lowerer_control", control_node->type);
    }

    lowerer_lox_ir_control(lllil_control);
}

void lower_lox_ir_control_exit_monitor(struct lllil_control * lllil) {
    struct lox_ir_control_node * control = lllil->control_node_to_lower;
    struct lox_ir_control_enter_monitor_node * enter_monitor_control_node = (struct lox_ir_control_enter_monitor_node *) control;
    struct lox_ir_block * block = enter_monitor_control_node->control.block;

    emit_function_call_ll_lox_ir(
            lllil, &set_self_thread_waiting, "set_self_thread_waiting", 0
    );

    emit_function_call_ll_lox_ir(
            lllil, &enter_monitor, "enter_monitor", 1, IMMEDIATE_TO_OPERAND((uint64_t) enter_monitor_control_node->monitor)
    );

    emit_function_call_ll_lox_ir(
            lllil, &set_self_thread_runnable, "set_self_thread_runnable", 0
    );
}

void lower_lox_ir_control_enter_monitor(struct lllil_control * lllil) {
    struct lox_ir_control_node * control = lllil->control_node_to_lower;
    struct lox_ir_control_exit_monitor_node * exit_monitor_control_node = (struct lox_ir_control_exit_monitor_node *) control;
    struct lox_ir_block * block = exit_monitor_control_node->control.block;

    emit_function_call_ll_lox_ir(
            lllil, &exit_monitor, "exit_monitor", 1, IMMEDIATE_TO_OPERAND((uint64_t) exit_monitor_control_node->monitor)
    );
}

void lower_lox_ir_control_print(struct lllil_control * lllil) {
    struct lox_ir_control_node * control = lllil->control_node_to_lower;
    struct lox_ir_control_print_node * print_node = (struct lox_ir_control_print_node *) control;
    struct lox_ir_ll_operand to_print_data = lower_lox_ir_data(lllil, NULL, print_node->data, NULL);
    struct lox_ir_block * block = control->block;

    if (is_lox_lox_ir_type(print_node->data->produced_type->type)) {
        emit_function_call_ll_lox_ir(lllil, &print_lox_value, "print_lox_value", 1, to_print_data);
    } else {
        char * formatted = print_node->data->produced_type->type == LOX_IR_TYPE_F64 ? "%g" : "%llu";
        emit_function_call_ll_lox_ir(lllil, &printf, "printf", 2, IMMEDIATE_TO_OPERAND((uint64_t) formatted), to_print_data);
    }
}

void lower_lox_ir_control_data(struct lllil_control * lllil_control) {
    struct lox_ir_control_node * control = lllil_control->control_node_to_lower;
    struct lox_ir_control_data_node * data_node = (struct lox_ir_control_data_node *) control;
    lower_lox_ir_data(lllil_control, NULL, data_node->data, NULL);
}

void lower_lox_ir_control_return(struct lllil_control * lllil_control) {
    struct lox_ir_control_node * control = lllil_control->control_node_to_lower;
    struct lox_ir_control_return_node * return_node = (struct lox_ir_control_return_node *) control;
    struct lox_ir_block * block = control->block;

    struct lox_ir_control_ll_return * low_level_return_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_RETURN,
            struct lox_ir_control_ll_return, control->block, LOX_IR_ALLOCATOR(lllil_control->lllil->lox_ir));
    low_level_return_node->empty_return = return_node->empty_return;

    if (!return_node->empty_return) {
        low_level_return_node->to_return = lower_lox_ir_data(lllil_control, NULL, return_node->data, NULL);
    }

    add_lowered_node_lllil_control(lllil_control, &low_level_return_node->control);
}

void lower_lox_ir_control_set_global(struct lllil_control * lllil) {
    struct lox_ir_control_node * control_node = lllil->control_node_to_lower;
    struct lox_ir_control_set_global_node * set_global_node = (struct lox_ir_control_set_global_node *) control_node;
    struct lox_ir_block * block = control_node->block;

    struct lox_ir_ll_operand new_global_value = lower_lox_ir_data(lllil, NULL, set_global_node->value_node, NULL);

    emit_function_call_ll_lox_ir(
            lllil,
            &put_hash_table,
            "put_hash_table",
            3,
            IMMEDIATE_TO_OPERAND((uint64_t) &set_global_node->package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) &set_global_node->name),
            new_global_value
    );
}

static void set_struct_field_escapes(struct lllil_control*, struct lox_ir_control_set_struct_field_node*);
static void set_struct_field_doesnt_escape(struct lllil_control*, struct lox_ir_control_set_struct_field_node*);

void lower_lox_ir_control_set_struct_field(struct lllil_control * lllil) {
    struct lox_ir_control_node * control_node = lllil->control_node_to_lower;
    struct lox_ir_control_set_struct_field_node * set_struct_field = (struct lox_ir_control_set_struct_field_node *) control_node;

    if (set_struct_field->escapes) {
        set_struct_field_escapes(lllil, set_struct_field);
    } else {
        set_struct_field_doesnt_escape(lllil, set_struct_field);
    }
}

static void set_struct_field_escapes(struct lllil_control * lllil, struct lox_ir_control_set_struct_field_node * set_struct_field) {
    struct lox_ir_block * block = set_struct_field->control.block;

    struct lox_ir_ll_operand instance = lower_lox_ir_data(lllil, NULL, set_struct_field->instance, NULL);

    struct lox_ir_ll_operand new_instance_value = lower_lox_ir_data(lllil, NULL, set_struct_field->new_field_value, NULL);

    emit_function_call_ll_lox_ir(
            lllil,
            &put_hash_table,
            "put_hash_table",
            3,
            instance,
            IMMEDIATE_TO_OPERAND((uint64_t) &set_struct_field->field_name),
            new_instance_value
    );
}

static void set_struct_field_doesnt_escape(struct lllil_control * lllil, struct lox_ir_control_set_struct_field_node * set_struct_field) {
    struct struct_definition_object * definition = set_struct_field->instance->produced_type->value.struct_instance->definition;

    //If it escapes, set_struct_field->instance should always produce pointer to values
    struct lox_ir_ll_operand instance = lower_lox_ir_data(lllil, NULL, set_struct_field->instance, NULL);
    struct lox_ir_ll_operand new_field_value = lower_lox_ir_data(lllil, NULL, set_struct_field->new_field_value, NULL);
    int new_value_offset = get_offset_field_struct_definition_ll_lox_ir(definition, set_struct_field->field_name->chars);

    emit_store_at_offset_ll_lox_ir(
            lllil, instance, new_value_offset, new_field_value
    );
}

void lower_lox_ir_control_set_array_element(struct lllil_control * lllil) {
    struct lox_ir_control_node * control = lllil->control_node_to_lower;
    struct lox_ir_control_set_array_element_node * set_array_element = (struct lox_ir_control_set_array_element_node *) control;
    struct lox_ir_ll_operand array_instance = lower_lox_ir_data(lllil, NULL, set_array_element->array, NULL);
    struct lox_ir_ll_operand index = lower_lox_ir_data(lllil, NULL, set_array_element->index, NULL);
    struct lox_ir_ll_operand new_value = lower_lox_ir_data(lllil, NULL, set_array_element->new_element_value, NULL);

    if (set_array_element->requires_range_check) {
        emit_range_check_ll_lox_ir(lllil, array_instance, index, set_array_element->escapes);
    }

    if (set_array_element->escapes) {
        emit_binary_ll_lox_ir(lllil, BINARY_LL_LOX_IR_IADD, array_instance, array_instance,
                              IMMEDIATE_TO_OPERAND(offsetof(struct array_object, values)));
    }

    if (index.type == LOX_IR_LL_OPERAND_IMMEDIATE) {
        emit_binary_ll_lox_ir(lllil, BINARY_LL_LOX_IR_IMUL, index, index, IMMEDIATE_TO_OPERAND(8));
        emit_store_at_offset_ll_lox_ir(lllil, array_instance, 0, new_value);
    } else {
        emit_store_at_offset_ll_lox_ir(lllil, array_instance, index.immedate * 8, new_value);
    }
}

void lower_lox_ir_control_loop(struct lllil_control * lllil) {
    //Ignore
}

void lower_lox_ir_control_conditional_jump(struct lllil_control * lllil) {
    struct lox_ir_control_node * control_node = lllil->control_node_to_lower;
    struct lox_ir_control_conditional_jump_node * cond_jump = (struct lox_ir_control_conditional_jump_node *) control_node;
    lower_lox_ir_data(lllil, NULL, cond_jump->condition, NULL);
}

void lower_lox_ir_control_guard(struct lllil_control * lllil) {
    struct lox_ir_control_node * control = lllil->control_node_to_lower;
    struct lox_ir_control_guard_node * guard_node = (struct lox_ir_control_guard_node *) control;
    struct lox_ir_guard guard = guard_node->guard;
    struct lox_ir_ll_operand guard_input = lower_lox_ir_data(lllil, NULL, guard_node->guard.value, NULL);
    emit_guard_ll_lox_ir(lllil, guard_input, guard);
}

void lower_lox_ir_control_define_ssa_name(struct lllil_control * lllil) {
    struct lox_ir_control_node * control = lllil->control_node_to_lower;
    struct lox_ir_control_define_ssa_name_node * define_ssa_name = (struct lox_ir_control_define_ssa_name_node *) control;
    struct v_register result = CREATE_V_REG(define_ssa_name->ssa_name, false);
    struct lox_ir_ll_operand new_value = lower_lox_ir_data(lllil, NULL, define_ssa_name->value, &result);

    if (!is_redundant_register_move(new_value, define_ssa_name)) {
        struct lox_ir_control_ll_move * move = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_MOVE,
                struct lox_ir_control_ll_move, control->block, LOX_IR_ALLOCATOR(lllil->lllil->lox_ir));
        move->to = V_REG_TO_OPERAND(CREATE_V_REG(define_ssa_name->ssa_name, false));
        move->from = new_value;

        add_lowered_node_lllil_control(lllil, &move->control);

    } else if (is_redundant_register_move(new_value, define_ssa_name) &&
            lowered_node_modifies_diferent_register_than_lowered_node_result(lllil, new_value)) {
        //This is the case when we modify one register by a unary operation, but we remove the definition, this can happen
        //in this case: i1 = i0 + 1
        replace_ssa_name_lox_ir(lllil->lllil->lox_ir, define_ssa_name->ssa_name,
                                GET_LL_UNARY_OPERAND(lllil->last_node_lowered).v_register.ssa_name);
    }
}

static bool lowered_node_modifies_diferent_register_than_lowered_node_result(
        struct lllil_control * lllil_control,
        struct lox_ir_ll_operand lowered_node_result
) {
    struct lox_ir_control_node * lowered_node = lllil_control->last_node_lowered;
    struct lox_ir_control_ll_unary * unary = lowered_node;
    return lowered_node->type == LOX_IR_CONTROL_NODE_LL_UNARY
        && lowered_node_result.type == LOX_IR_LL_OPERAND_REGISTER
        && GET_LL_UNARY_OPERAND(lowered_node).type == LOX_IR_LL_OPERAND_REGISTER
        && GET_LL_UNARY_OPERAND(lowered_node).v_register.ssa_name.u16 != lowered_node_result.v_register.ssa_name.u16;
}

static bool lowered_unary_node(struct lllil_control * lllil_control) {
    return lllil_control->last_node_lowered->type == LOX_IR_CONTROL_NODE_LL_UNARY;
}

static bool is_redundant_register_move(struct lox_ir_ll_operand operand, struct lox_ir_control_define_ssa_name_node * define) {
    return operand.type == LOX_IR_LL_OPERAND_REGISTER
        && operand.v_register.ssa_name.u16 == define->ssa_name.u16;
}