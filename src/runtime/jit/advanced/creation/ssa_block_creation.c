#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/ssa_control_node.h"
#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/stack_list.h"

typedef enum {
    PARENT_NEXT_TYPE_TRUE_BRANCH,
    PARENT_NEXT_TYPE_FALSE_BRANCH,
    PARENT_NEXT_TYPE_LOOP,
    PARENT_NEXT_TYPE_SEQ,
} parent_next_type_t;

struct pending_evaluate {
    parent_next_type_t parent_next_type;
    struct ssa_block * parent_block;
    struct ssa_control_node * pending_to_evalute;
};

struct block_local_usage {
    struct u8_arraylist inputs;
    struct u8_arraylist outputs;
};

static struct block_local_usage get_block_local_variables_usage(struct ssa_control_node *, struct ssa_control_node *);
static struct ssa_block * create_parent_block(struct ssa_control_node *);
static struct ssa_control_node * get_last_node_in_block(struct ssa_control_node *start);
static void attatch_new_block_to_parent_block(struct ssa_block * parent, struct ssa_block * new_block, parent_next_type_t);
static void get_input_outputs_from_control_node(struct u8_arraylist * input, struct u8_arraylist * output, struct ssa_control_node *);

static void push_pending_evaluate(
        struct stack_list * pending_evaluation_stack,
        parent_next_type_t type_parent_next_block,
        struct ssa_block * parent_block,
        struct ssa_control_node * pending_to_evalute
);

struct ssa_block * create_ssa_ir_blocks(
        struct ssa_control_node * start
) {
    struct stack_list pending_evalute;
    init_stack_list(&pending_evalute);

    struct ssa_block * start_ssa_block = create_parent_block(start);
    push_pending_evaluate(&pending_evalute, PARENT_NEXT_TYPE_SEQ, start_ssa_block, start->next.next);

    while(!is_empty_stack_list(&pending_evalute)){
        struct pending_evaluate * to_evalute = pop_stack_list(&pending_evalute);
        parent_next_type_t parent_next_type = to_evalute->parent_next_type;
        struct ssa_control_node * pending_to_evalute = to_evalute->pending_to_evalute;
        struct ssa_block * parent_block = to_evalute->parent_block;
        free(to_evalute);

        struct ssa_control_node * first_node = pending_to_evalute;
        struct ssa_control_node * last_node = get_last_node_in_block(first_node);
        struct block_local_usage local_variables_usage = get_block_local_variables_usage(first_node, last_node);
        struct u8_arraylist inputs = local_variables_usage.inputs;
        struct u8_arraylist outputs = local_variables_usage.outputs;
        type_next_ssa_block_t type_next_ssa_block = get_type_next_ssa_block(last_node);

        struct ssa_block * block = alloc_ssa_block();
        block->type_next_ssa_block = type_next_ssa_block;
        block->first = first_node;
        block->last = last_node;
        block->outputs = outputs;
        block->inputs = inputs;

        //Push the next node to the pending evalution stack
        switch(type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ:
                push_pending_evaluate(&pending_evalute, PARENT_NEXT_TYPE_SEQ, block, last_node->next.next);
                break;
            case TYPE_NEXT_SSA_BLOCK_LOOP:
                push_pending_evaluate(&pending_evalute, PARENT_NEXT_TYPE_LOOP, block, last_node->next.next);
                break;
            case TYPE_NEXT_SSA_BLOCK_BRANCH:
                push_pending_evaluate(&pending_evalute, PARENT_NEXT_TYPE_TRUE_BRANCH, block, last_node->next.branch.true_branch);
                push_pending_evaluate(&pending_evalute, PARENT_NEXT_TYPE_FALSE_BRANCH, block, last_node->next.branch.false_branch);
                break;
            case TYPE_NEXT_SSA_BLOCK_NONE:
                break;
        }

        attatch_new_block_to_parent_block(parent_block, block, parent_next_type);
    }

    return start_ssa_block;
}

bool is_sequential_ssa_control_node_type(ssa_control_node_type type) {
    bool has_branches = type == SSA_CONTROL_NODE_TYPE_RETURN ||
                        type == SSA_CONTROL_NODE_TYPE_LOOP_JUMP ||
                        type == SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP;
    return !has_branches;
}

static struct ssa_control_node * get_last_node_in_block(struct ssa_control_node * start) {
    struct ssa_control_node * current = start;
    while (is_sequential_ssa_control_node_type(current->next.next->type)) {
        current = current->next.next;
    }
    return current;
}

static void push_pending_evaluate(
        struct stack_list * pending_evaluation_stack,
        parent_next_type_t parent_next_type,
        struct ssa_block * parent_block,
        struct ssa_control_node * pending_to_evalute
) {
    struct pending_evaluate * pending_evaluate = malloc(sizeof(struct pending_evaluate));
    pending_evaluate->parent_next_type = parent_next_type;
    pending_evaluate->parent_block = parent_block;
    pending_evaluate->pending_to_evalute = pending_to_evalute;
    push_stack_list(pending_evaluation_stack, pending_evaluate);
}

static struct ssa_block * create_parent_block(struct ssa_control_node * start_ssa_node) {
    struct ssa_block * parent_block = alloc_ssa_block();
    parent_block->first = start_ssa_node;
    parent_block->last = start_ssa_node;
    parent_block->type_next_ssa_block = TYPE_NEXT_SSA_BLOCK_SEQ;
    return parent_block;
}

static void attatch_new_block_to_parent_block(
        struct ssa_block * parent,
        struct ssa_block * new_block,
        parent_next_type_t parent_next_type
) {
    switch(parent_next_type) {
        case PARENT_NEXT_TYPE_TRUE_BRANCH:
            parent->next.branch.true_branch = new_block;
            break;
        case PARENT_NEXT_TYPE_FALSE_BRANCH:
            parent->next.branch.false_branch = new_block;
            break;
        case PARENT_NEXT_TYPE_LOOP:
            parent->next.loop = new_block;
            break;
        case PARENT_NEXT_TYPE_SEQ:
            parent->next.next = new_block;
            break;
    }
}

static struct block_local_usage get_block_local_variables_usage(
        struct ssa_control_node * first_node,
        struct ssa_control_node * last_node
) {
    struct u8_arraylist input;
    struct u8_arraylist output;
    init_u8_arraylist(&input);
    init_u8_arraylist(&output);

    for (struct ssa_control_node * current = first_node; current <= last_node; current = current->next.next) {
        get_input_outputs_from_control_node(&input, &output, current);
    }

    remove_duplicates_u8_arraylist(&output);
    remove_duplicates_u8_arraylist(&input);

    return (struct block_local_usage) {
        .outputs = output,
        .inputs = input,
    };
}

static void get_input_outputs_from_control_node(
        struct u8_arraylist * input,
        struct u8_arraylist * output,
        struct ssa_control_node * node
) {
    switch (node->type) {
        case SSA_CONTROL_NODE_TYPE_DATA: {
            struct ssa_control_data_node * data_node = (struct ssa_control_data_node *) node;
            get_assgined_locals(output, data_node->data);
            get_used_locals(input, data_node->data);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) node;
            get_assgined_locals(output, return_node->data);
            get_used_locals(input, return_node->data);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) node;
            get_assgined_locals(output, print_node->data);
            get_used_locals(input, print_node->data);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) node;
            get_assgined_locals(output, set_global->value_node);
            get_used_locals(input, set_global->value_node);
            break;
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct = (struct ssa_data_get_struct_field_node *) node;
            get_assgined_locals(output, set_global->value_node);
            get_used_locals(input, set_global->value_node);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array = (struct ssa_control_set_array_element_node *) node;
            get_assgined_locals(output, set_array->array);
            get_used_locals(input, set_array->array);
            get_assgined_locals(output, set_array->new_element);
            get_used_locals(input, set_array->new_element);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP: {
            struct ssa_control_conditional_jump_node * conditional_jump = (struct ssa_control_conditional_jump_node *) node;
            get_assgined_locals(output, conditional_jump->condition);
            get_used_locals(input, conditional_jump->condition);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_START:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
            break;
    }
}