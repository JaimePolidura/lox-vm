#include "ir_lowerer.h"

struct llil {
    struct lox_ir * lox_ir;
    struct arena_lox_allocator llil_allocator;
};

typedef void(* lowerer_lox_ir_control_t)(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_data(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_return(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_print(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_exit_monitor(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_enter_monitor(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_set_global(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_set_struct_field(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_set_array_element(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_loop(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_conditional_jump(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_guard(struct llil*, struct lox_ir_control_node*);
void lowerer_lox_ir_control_set_v_reg(struct llil*, struct lox_ir_control_node*);

extern void enter_monitor(struct monitor * monitor);
extern void exit_monitor(struct monitor * monitor);
extern void set_self_thread_runnable();
extern void set_self_thread_waiting();

lowerer_lox_ir_control_t lowerer_lox_ir_by_control_node[] = {
        [LOX_IR_CONTROL_NODE_DATA] = lowerer_lox_ir_control_data,
        [LOX_IR_CONTROL_NODE_RETURN] = lowerer_lox_ir_control_return,
        [LOX_IR_CONTROL_NODE_PRINT] = lowerer_lox_ir_control_print,
        [LOX_IR_CONTROL_NODE_ENTER_MONITOR] = lowerer_lox_ir_control_exit_monitor, //Ok
        [LOX_IR_CONTROL_NODE_EXIT_MONITOR] = lowerer_lox_ir_control_enter_monitor, //Ok
        [LOX_IR_CONTORL_NODE_SET_GLOBAL] = lowerer_lox_ir_control_set_global,
        [LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD] = lowerer_lox_ir_control_set_struct_field,
        [LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT] = lowerer_lox_ir_control_set_array_element,
        [LOX_IR_CONTROL_NODE_LOOP_JUMP] = lowerer_lox_ir_control_loop,
        [LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP] = lowerer_lox_ir_control_conditional_jump,
        [LOX_IR_CONTROL_NODE_GUARD] = lowerer_lox_ir_control_guard,
        [LOX_IR_CONTROL_NODE_SET_V_REGISTER] = lowerer_lox_ir_control_set_v_reg,
};

bool lower_lox_ir_block(struct lox_ir_block *, void *);

static struct llil * alloc_low_level_lox_ir_lowerer(struct lox_ir *);
static void free_low_level_lox_ir_lowerer(struct llil *);
static void lower_lox_ir_control(struct llil *, struct lox_ir_control_node *);

struct lox_level_lox_ir_node * lower_lox_ir(struct lox_ir * lox_ir) {
    struct llil * llil = alloc_low_level_lox_ir_lowerer(lox_ir);

    for_each_lox_ir_block(
            lox_ir->first_block,
            NATIVE_LOX_ALLOCATOR(),
            llil,
            LOX_IR_BLOCK_OPT_NOT_REPEATED,
            lower_lox_ir_block
    );

    free_low_level_lox_ir_lowerer(llil);

    return NULL;
}

bool lower_lox_ir_block(struct lox_ir_block * block, void * extra) {
    struct llil * llil = extra;

    for (struct lox_ir_control_node * current = block->first;;current = current->next) {
        lower_lox_ir_control(llil, current);

        if (current == block->last) {
            break;
        }
    }

    return true;
}

static void lower_lox_ir_control(struct llil * llil, struct lox_ir_control_node * cotrol_node) {
    lowerer_lox_ir_control_t lowerer_lox_ir_control = lowerer_lox_ir_by_control_node[cotrol_node->type];
    lowerer_lox_ir_control(llil, cotrol_node);
}

void lowerer_lox_ir_control_exit_monitor(struct llil * llil, struct lox_ir_control_node * control) {
    struct lox_ir_control_enter_monitor_node * enter_monitor_control_node = (struct lox_ir_control_enter_monitor_node *) control;
    struct lox_ir_block * block = enter_monitor_control_node->control.block;

    struct lox_ir_control_ll_function_call * set_self_thread_waiting = create_ll_function_call_lox_ir_control(
            &llil->llil_allocator.lox_allocator,
            control->block,
            &set_self_thread_waiting,
            0
    );

    struct lox_ir_control_ll_function_call * enter_monitor = create_ll_function_call_lox_ir_control(
            &llil->llil_allocator.lox_allocator,
            control->block,
            &enter_monitor,
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) enter_monitor_control_node->monitor)
    );

    struct lox_ir_control_ll_function_call * set_self_thread_runnable = create_ll_function_call_lox_ir_control(
            &llil->llil_allocator.lox_allocator,
            control->block,
            &set_self_thread_runnable,
            0
    );

    replace_control_node_lox_ir_block(block, control, &set_self_thread_waiting->node);
    add_after_control_node_lox_ir_block(block, &set_self_thread_waiting->node, &enter_monitor->node);
    add_after_control_node_lox_ir_block(block, &enter_monitor->node, &set_self_thread_runnable->node);
}

void lowerer_lox_ir_control_enter_monitor(struct llil * llil, struct lox_ir_control_node * control) {
    struct lox_ir_control_exit_monitor_node * exit_monitor_control_node = (struct lox_ir_control_exit_monitor_node *) control;
    struct lox_ir_block * block = exit_monitor_control_node->control.block;

    struct lox_ir_control_ll_function_call * exit_monitor = create_ll_function_call_lox_ir_control(
            &llil->llil_allocator.lox_allocator,
            control->block,
            &exit_monitor,
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) exit_monitor_control_node->monitor)
    );

    replace_control_node_lox_ir_block(block, control, &exit_monitor->node);
}

void lowerer_lox_ir_control_data(struct llil *, struct lox_ir_control_node * control) {

}
void lowerer_lox_ir_control_return(struct llil * llil, struct lox_ir_control_node * control) {

}

void lowerer_lox_ir_control_print(struct llil*, struct lox_ir_control_node*) {

}

void lowerer_lox_ir_control_set_global(struct llil*, struct lox_ir_control_node*) {

}

void lowerer_lox_ir_control_set_struct_field(struct llil*, struct lox_ir_control_node*) {

}

void lowerer_lox_ir_control_set_array_element(struct llil*, struct lox_ir_control_node*) {

}

void lowerer_lox_ir_control_loop(struct llil*, struct lox_ir_control_node*) {

}

void lowerer_lox_ir_control_conditional_jump(struct llil*, struct lox_ir_control_node*) {

}
void lowerer_lox_ir_control_guard(struct llil*, struct lox_ir_control_node*) {

}

void lowerer_lox_ir_control_set_v_reg(struct llil*, struct lox_ir_control_node*) {

}

static struct llil * alloc_low_level_lox_ir_lowerer(struct lox_ir * lox_ir) {
    struct llil * llil = NATIVE_LOX_MALLOC(sizeof(struct llil));
    struct arena arena;
    llil->lox_ir = lox_ir;
    init_arena(&arena);
    llil->llil_allocator = to_lox_allocator_arena(arena);
    return llil;
}

static void free_low_level_lox_ir_lowerer(struct llil * llil) {
    free_arena(&llil->llil_allocator.arena);
    NATIVE_LOX_FREE(llil);
}