#include "ir_lowerer_control.h"

typedef void(* lowerer_lox_ir_control_t)(struct lllil*, struct lox_ir_control_node*);
void lower_lox_ir_control_data(struct lllil *lllil, struct lox_ir_control_node *control);
void lower_lox_ir_control_return(struct lllil *llil, struct lox_ir_control_node *control);
void lower_lox_ir_control_print(struct lllil *lllil, struct lox_ir_control_node *control);
void lower_lox_ir_control_exit_monitor(struct lllil *lllil, struct lox_ir_control_node *control);
void lower_lox_ir_control_enter_monitor(struct lllil *lllil, struct lox_ir_control_node *control);
void lower_lox_ir_control_set_global(struct lllil*, struct lox_ir_control_node*);
void lower_lox_ir_control_set_struct_field(struct lllil*, struct lox_ir_control_node*);
void lower_lox_ir_control_set_array_element(struct lllil*, struct lox_ir_control_node*);
void lower_lox_ir_control_loop(struct lllil*, struct lox_ir_control_node*);
void lower_lox_ir_control_conditional_jump(struct lllil*, struct lox_ir_control_node*);
void lower_lox_ir_control_guard(struct lllil*, struct lox_ir_control_node*);
void lower_lox_ir_control_set_v_reg(struct lllil*, struct lox_ir_control_node*);

extern void enter_monitor(struct monitor * monitor);
extern void exit_monitor(struct monitor * monitor);
extern void set_self_thread_runnable();
extern void set_self_thread_waiting();
extern void print_lox_value(lox_value_t value);
extern void print_lox_value(lox_value_t value);
extern bool put_hash_table(struct lox_hash_table*, struct string_object*, lox_value_t);

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
        [LOX_IR_CONTROL_NODE_SET_V_REGISTER] = lower_lox_ir_control_set_v_reg,
};

//Main function, entry point
void lower_lox_ir_control(struct lllil * lllil, struct lox_ir_control_node * control_node) {
    lowerer_lox_ir_control_t lowerer_lox_ir_control = lowerer_lox_ir_by_control_node[control_node->type];
    lowerer_lox_ir_control(lllil, control_node);
}

void lower_lox_ir_control_exit_monitor(struct lllil * lllil, struct lox_ir_control_node * control) {
    struct lox_ir_control_enter_monitor_node * enter_monitor_control_node = (struct lox_ir_control_enter_monitor_node *) control;
    struct lox_ir_block * block = enter_monitor_control_node->control.block;

    emit_function_call_ll_lox_ir(
            lllil, control, &set_self_thread_waiting, 0
    );

    emit_function_call_ll_lox_ir(
            lllil, control, &enter_monitor, 1, IMMEDIATE_TO_OPERAND((uint64_t) enter_monitor_control_node->monitor)
    );

    emit_function_call_ll_lox_ir(
            lllil, control, &set_self_thread_runnable, 0
    );
}

void lower_lox_ir_control_enter_monitor(struct lllil * lllil, struct lox_ir_control_node * control) {
    struct lox_ir_control_exit_monitor_node * exit_monitor_control_node = (struct lox_ir_control_exit_monitor_node *) control;
    struct lox_ir_block * block = exit_monitor_control_node->control.block;

    emit_function_call_ll_lox_ir(
            lllil, control, &exit_monitor, 1, IMMEDIATE_TO_OPERAND((uint64_t) exit_monitor_control_node->monitor)
    );
}

void lower_lox_ir_control_print(struct lllil * lllil, struct lox_ir_control_node * control) {
    struct lox_ir_control_print_node * print_node = (struct lox_ir_control_print_node *) control;
    struct lox_ir_ll_operand to_print_data = lower_lox_ir_data(lllil, print_node->data, LOX_IR_TYPE_LOX_ANY);
    struct lox_ir_block * block = control->block;

    if (is_lox_lox_ir_type(print_node->data->produced_type->type)) {
        emit_function_call_ll_lox_ir(lllil, control, &print_lox_value, 1, to_print_data);
    } else {
        char * formatted = print_node->data->produced_type->type == LOX_IR_TYPE_F64 ? "%g" : "%llu";
        emit_function_call_ll_lox_ir(lllil, control, &printf, 2, IMMEDIATE_TO_OPERAND((uint64_t) formatted), to_print_data);
    }
}

void lower_lox_ir_control_data(struct lllil * lllil, struct lox_ir_control_node * control) {
    struct lox_ir_control_data_node * data_node = (struct lox_ir_control_data_node *) control;
    lower_lox_ir_data(lllil, data_node->data, LOX_IR_TYPE_LOX_ANY);
}

void lower_lox_ir_control_return(struct lllil * llil, struct lox_ir_control_node * control) {
    struct lox_ir_control_return_node * return_node = (struct lox_ir_control_return_node *) control;
    struct lox_ir_block * block = control->block;

    struct lox_ir_control_ll_return * low_level_return_node = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_LL_RETURN, struct lox_ir_control_ll_return, control->block, &llil->lllil_allocator.lox_allocator
    );
    low_level_return_node->emtpy_return = return_node->empty_return;
    if(!return_node->empty_return){
        low_level_return_node->to_return = lower_lox_ir_data(llil, return_node->data, LOX_IR_TYPE_LOX_ANY);
    }

    replace_control_node_lox_ir_block(block, control, &low_level_return_node->node);
}

void lower_lox_ir_control_set_global(struct lllil * lllil, struct lox_ir_control_node * control) {
    struct lox_ir_control_set_global_node * set_global_node = (struct lox_ir_control_set_global_node *) control;
    struct lox_ir_block * block = control->block;

    struct lox_ir_ll_operand new_global_value = lower_lox_ir_data(lllil, set_global_node->value_node, LOX_IR_TYPE_LOX_ANY);

    emit_function_call_ll_lox_ir(
            lllil,
            control,
            &put_hash_table,
            3,
            IMMEDIATE_TO_OPERAND((uint64_t) &set_global_node->package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) &set_global_node->name),
            new_global_value
    );
}

static void set_struct_field_escapes(struct lllil*, struct lox_ir_control_set_struct_field_node*);
static void set_struct_field_doesnt_escape(struct lllil*, struct lox_ir_control_set_struct_field_node*);

void lower_lox_ir_control_set_struct_field(struct lllil * lllil, struct lox_ir_control_node * control) {
    struct lox_ir_control_set_struct_field_node * set_struct_field = (struct lox_ir_control_set_struct_field_node *) control;

    if (!set_struct_field->escapes) {
        set_struct_field_escapes(lllil, set_struct_field);
    } else {
        set_struct_field_doesnt_escape(lllil, set_struct_field);
    }
}

static void set_struct_field_escapes(struct lllil * lllil, struct lox_ir_control_set_struct_field_node * set_struct_field) {
    struct lox_ir_block * block = set_struct_field->control.block;

    struct lox_ir_ll_operand instance = lower_lox_ir_data(lllil, set_struct_field->instance,
            LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE);
    struct lox_ir_ll_operand instance_values_map = emit_load_at_offset_ll_lox_ir(
            lllil, &set_struct_field->control, instance, offsetof(struct struct_instance_object, fields)
    );

    struct lox_ir_ll_operand new_instance_value = lower_lox_ir_data(lllil, set_struct_field->new_field_value,
            LOX_IR_TYPE_LOX_ANY);

    emit_function_call_ll_lox_ir(
            lllil,
            &set_struct_field->control,
            &put_hash_table,
            3,
            instance_values_map,
            IMMEDIATE_TO_OPERAND((uint64_t) &set_struct_field->field_name),
            new_instance_value
    );
}

static void set_struct_field_doesnt_escape(struct lllil * lllil, struct lox_ir_control_set_struct_field_node * set_struct_field_node) {
    struct struct_definition_object * definition = set_struct_field_node->instance->produced_type->value.struct_instance->definition;

    struct lox_ir_ll_operand instance = lower_lox_ir_data(lllil, set_struct_field_node->instance,
            LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE);
    struct lox_ir_ll_operand new_field_value = lower_lox_ir_data(lllil, set_struct_field_node->new_field_value,
            LOX_IR_TYPE_LOX_ANY);
    int new_value_offset = get_offset_field_struct_definition_ll_lox_ir(definition, set_struct_field_node->field_name->chars);

    emit_store_at_offset_ll_lox_ir(
            lllil, &set_struct_field_node->control, instance, new_value_offset, new_field_value
    );
}

void lower_lox_ir_control_set_array_element(struct lllil * lllil, struct lox_ir_control_node * control) {
    struct lox_ir_control_set_array_element_node * set_array_element = (struct lox_ir_control_set_array_element_node *) control;
    struct lox_ir_ll_operand array_instance = lower_lox_ir_data(lllil, set_array_element->array, LOX_IR_TYPE_NATIVE_ARRAY);
    struct lox_ir_ll_operand index = lower_lox_ir_data(lllil, set_array_element->index, LOX_IR_TYPE_NATIVE_I64);
    struct lox_ir_ll_operand new_value = lower_lox_ir_data(lllil, set_array_element->new_element_value, LOX_IR_TYPE_LOX_ANY);

    if (set_array_element->requires_range_check) {
        emit_range_check_ll_lox_ir(lllil, set_array_element, array_instance, index);
    }

    if (set_array_element->escapes) {
        emit_iadd_ll_lox_ir(lllil, control, array_instance, IMMEDIATE_TO_OPERAND(offsetof(struct array_object, values)));
    }

    if (index.type == LOX_IR_LL_OPERAND_IMMEDIATE) {
        emit_imul_ll_lox_ir(lllil, control, index, IMMEDIATE_TO_OPERAND(8));
        emit_store_at_offset_ll_lox_ir(lllil, control, array_instance, 0, new_value);
    } else {
        emit_store_at_offset_ll_lox_ir(lllil, control, array_instance, index.immedate * 8, new_value);
    }
}

void lower_lox_ir_control_loop(struct lllil*, struct lox_ir_control_node*) {
    //Ignore
}

void lower_lox_ir_control_conditional_jump(struct lllil * lllil, struct lox_ir_control_node * control_node) {
    struct lox_ir_control_conditional_jump_node * cond_jump = (struct lox_ir_control_conditional_jump_node *) control_node;
    lower_lox_ir_data(lllil, cond_jump->condition, LOX_IR_TYPE_NATIVE_BOOLEAN);
}

void lower_lox_ir_control_guard(struct lllil * lllil, struct lox_ir_control_node * control) {
    struct lox_ir_control_guard_node * guard_node = (struct lox_ir_control_guard_node *) control;
    struct lox_ir_guard guard = guard_node->guard;
    emit_guard_ll_lox_ir(lllil, control, guard);
}

void lower_lox_ir_control_set_v_reg(struct lllil * lllil, struct lox_ir_control_node * control) {
    struct lox_ir_control_set_v_register_node * set_v_reg = (struct lox_ir_control_set_v_register_node *) control;

    struct lox_ir_ll_operand new_value = lower_lox_ir_data(lllil, set_v_reg->value, LOX_IR_TYPE_LOX_ANY);

    struct lox_ir_control_ll_move * move = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LL_MOVE,
            struct lox_ir_control_ll_move, control->block, LOX_IR_ALLOCATOR(lllil->lox_ir));

    move->to = V_REG_TO_OPERAND(set_v_reg->v_register);
    move->from = new_value;

    replace_control_node_lox_ir_block(control->block, control, &move->node);
}