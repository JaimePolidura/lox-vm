#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u64_hash_table.h"
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
    struct u8_set inputs;
    struct u8_set outputs;
};

static struct block_local_usage get_block_local_variables_usage(struct ssa_control_node *, struct ssa_control_node *);
static struct ssa_block * create_parent_block(struct ssa_control_node *);
static struct ssa_control_node * get_last_node_in_block(struct ssa_control_node *start);
static void attatch_new_block_to_parent_block(struct ssa_block * parent, struct ssa_block * new_block, parent_next_type_t);
static void get_input_outputs_from_control_node(struct u8_set * input, struct u8_set * output, struct ssa_control_node *);
static void map_ssa_nodes_to_ssa_blocks(struct u64_hash_table *, struct ssa_block *, struct ssa_control_node * first, struct ssa_control_node * last);

static void push_pending_evaluate(
        struct stack_list * pending_evaluation_stack,
        parent_next_type_t type_parent_next_block,
        struct ssa_block * parent_block,
        struct ssa_control_node * pending_to_evalute
);

struct ssa_block * create_ssa_ir_blocks(
        struct ssa_control_node * start
) {
    struct u64_hash_table block_by_ssa_control_nodes;
    struct stack_list pending_evalute;

    init_u64_hash_table(&block_by_ssa_control_nodes);
    init_stack_list(&pending_evalute);

    struct ssa_block * start_ssa_block = create_parent_block(start);
    push_pending_evaluate(&pending_evalute, PARENT_NEXT_TYPE_SEQ, start_ssa_block, start->next.next);

    while (!is_empty_stack_list(&pending_evalute)) {
        struct pending_evaluate * to_evalute = pop_stack_list(&pending_evalute);
        parent_next_type_t parent_next_type = to_evalute->parent_next_type;
        struct ssa_control_node * pending_node_to_evalute = to_evalute->pending_to_evalute;
        struct ssa_block * parent_block = to_evalute->parent_block;
        free(to_evalute);

        if (contains_u64_hash_table(&block_by_ssa_control_nodes, (uint64_t) pending_node_to_evalute)) {
            struct ssa_block * ssa_block_evaluated = (struct ssa_block *) get_u64_hash_table(&block_by_ssa_control_nodes, (uint64_t) pending_node_to_evalute);
            attatch_new_block_to_parent_block(parent_block, ssa_block_evaluated, parent_next_type);
            continue;
        }

        struct ssa_control_node * first_node = pending_node_to_evalute;
        struct ssa_control_node * last_node = get_last_node_in_block(first_node);
        struct block_local_usage local_variables_usage = get_block_local_variables_usage(first_node, last_node);
        struct u8_set inputs = local_variables_usage.inputs;
        struct u8_set outputs = local_variables_usage.outputs;
        type_next_ssa_block_t type_next_ssa_block = get_type_next_ssa_block(last_node);

        struct ssa_block * block = alloc_ssa_block();
        block->type_next_ssa_block = type_next_ssa_block;
        block->first = first_node;
        block->outputs = outputs;
        block->last = last_node;
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
                push_pending_evaluate(&pending_evalute, PARENT_NEXT_TYPE_FALSE_BRANCH, block, last_node->next.branch.false_branch);
                push_pending_evaluate(&pending_evalute, PARENT_NEXT_TYPE_TRUE_BRANCH, block, last_node->next.branch.true_branch);
                break;
            case TYPE_NEXT_SSA_BLOCK_NONE:
                break;
        }

        attatch_new_block_to_parent_block(parent_block, block, parent_next_type);
        map_ssa_nodes_to_ssa_blocks(&block_by_ssa_control_nodes, block, first_node, last_node);
    }

    free_u64_hash_table(&block_by_ssa_control_nodes);
    free_stack_list(&pending_evalute);

    return start_ssa_block;
}

//The returned node should belong to the block AKA Inclusive
static struct ssa_control_node * get_last_node_in_block(struct ssa_control_node * start) {
    struct ssa_control_node * current = start;
    struct ssa_control_node * prev = current;

    while (current != NULL) {
        if(current->jumps_to_next_node) {
            return current;
        }

        switch (current->type) {
            case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
            case SSA_CONTROL_NODE_TYPE_RETURN:
                return current;
            case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
                struct ssa_control_conditional_jump_node * cond_jump = (struct ssa_control_conditional_jump_node *) current;
                return cond_jump->loop_condition ? prev : current;
            default:
                break;
        }

        prev = current;
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
    struct u8_set input;
    struct u8_set output;
    init_u8_set(&input);
    init_u8_set(&output);

    for (struct ssa_control_node * current = first_node;; current = current->next.next) {
        get_input_outputs_from_control_node(&input, &output, current);

        if(current == last_node){
            break;
        }
    }

    return (struct block_local_usage) {
        .outputs = output,
        .inputs = input,
    };
}

static void get_input_outputs_from_control_node(
        struct u8_set * input,
        struct u8_set * output,
        struct ssa_control_node * node
) {
    switch (node->type) {
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL: {
            struct ssa_control_set_local_node * set_local_node = (struct ssa_control_set_local_node *) node;
            add_u8_set(output, set_local_node->local_number);
            get_used_locals(input, set_local_node->new_local_value);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_DATA: {
            struct ssa_control_data_node * data_node = (struct ssa_control_data_node *) node;
            get_used_locals(input, data_node->data);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) node;
            get_used_locals(input, return_node->data);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) node;
            get_used_locals(input, print_node->data);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL: {
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) node;
            get_used_locals(input, set_global->value_node);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct = (struct ssa_control_set_struct_field_node *) node;
            get_used_locals(input, set_struct->field_value);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array = (struct ssa_control_set_array_element_node *) node;
            get_used_locals(input, set_array->array);
            get_used_locals(input, set_array->new_element);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP: {
            struct ssa_control_conditional_jump_node * conditional_jump = (struct ssa_control_conditional_jump_node *) node;
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

static void map_ssa_nodes_to_ssa_blocks(
        struct u64_hash_table * hashtable_mapping,
        struct ssa_block * block,
        struct ssa_control_node * first,
        struct ssa_control_node * last
) {
    struct ssa_control_node * current = first;
    while (current != NULL) {
        put_u64_hash_table(hashtable_mapping, (uint64_t) first, block);
        if(current == last) {
            break;
        }

        current = current->next.next;
    }
}