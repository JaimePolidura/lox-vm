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
    struct u8_set use_before_assigment;
    struct u8_set assigned;
};

static struct block_local_usage get_block_local_variables_usage(struct ssa_control_node *, struct ssa_control_node *);
static struct ssa_block * create_parent_block(struct ssa_control_node *);
static struct ssa_control_node * get_last_node_in_block(struct ssa_control_node *start);
static void attatch_new_block_to_parent_block(struct ssa_block * parent, struct ssa_block * new_block, parent_next_type_t);
static struct block_local_usage get_local_usage_in_control_node(struct ssa_control_node *node, struct u8_set use_before_assigment, struct u8_set assigned);
static void map_ssa_nodes_to_ssa_blocks(struct u64_hash_table *, struct ssa_block *, struct ssa_control_node * first, struct ssa_control_node * last);
static struct u8_set calculate_inputs_set(struct ssa_data_node *, struct u8_set prev_outputs, struct u8_set prev_inputs);
static struct u8_set calculate_use_before_assignment_set(struct ssa_data_node *, struct u8_set prev_outputs);
static bool is_loop_body(struct ssa_block * parent_block, parent_next_type_t type_parent_next_block);

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
        struct u8_set use_before_assigment = local_variables_usage.use_before_assigment;
        type_next_ssa_block_t type_next_ssa_block = get_type_next_ssa_block(last_node);

        struct ssa_block * block = alloc_ssa_block();
        block->loop_body = is_loop_body(parent_block, parent_next_type);
        block->use_before_assigment = use_before_assigment;
        block->type_next_ssa_block = type_next_ssa_block;
        block->first = first_node;
        block->last = last_node;

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
    struct u8_set use_before_assigment;
    struct u8_set assigned;
    init_u8_set(&use_before_assigment);
    init_u8_set(&assigned);

    for (struct ssa_control_node * current = first_node;; current = current->next.next) {
        struct block_local_usage local_usage = get_local_usage_in_control_node(current, use_before_assigment, assigned);

        union_u8_set(&use_before_assigment, local_usage.use_before_assigment);
        union_u8_set(&assigned, local_usage.assigned);

        if(current == last_node){
            break;
        }
    }

    //Keep only used variables
    intersection_u8_set(&use_before_assigment, assigned);

    return (struct block_local_usage) {
        .use_before_assigment = use_before_assigment,
        .assigned = assigned,
    };
}

static struct block_local_usage get_local_usage_in_control_node(
        struct ssa_control_node * node,
        struct u8_set use_before_assigment,
        struct u8_set assigned
) {
    switch (node->type) {
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL: {
            struct ssa_control_set_local_node * set_local_node = (struct ssa_control_set_local_node *) node;
            use_before_assigment = calculate_use_before_assignment_set(set_local_node->new_local_value, assigned);
            add_u8_set(&assigned, set_local_node->local_number);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_DATA: {
            struct ssa_control_data_node * data_node = (struct ssa_control_data_node *) node;
            use_before_assigment = calculate_use_before_assignment_set(data_node->data, assigned);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) node;
            use_before_assigment = calculate_use_before_assignment_set(return_node->data, assigned);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) node;
            use_before_assigment = calculate_use_before_assignment_set(print_node->data, assigned);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL: {
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) node;
            use_before_assigment = calculate_use_before_assignment_set(set_global->value_node, assigned);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct = (struct ssa_control_set_struct_field_node *) node;
            union_u8_set(&use_before_assigment, calculate_use_before_assignment_set(set_struct->instance, assigned));
            use_before_assigment = calculate_use_before_assignment_set(set_struct->new_field_value, assigned);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array = (struct ssa_control_set_array_element_node *) node;
            union_u8_set(&use_before_assigment, calculate_use_before_assignment_set(set_array->new_element_value, assigned));
            use_before_assigment = calculate_use_before_assignment_set(set_array->array, assigned);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP: {
            struct ssa_control_conditional_jump_node * conditional_jump = (struct ssa_control_conditional_jump_node *) node;
            use_before_assigment = calculate_use_before_assignment_set(conditional_jump->condition, assigned);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_START:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
            break;
    }

    return (struct block_local_usage) {
        .use_before_assigment = use_before_assigment,
        .assigned = assigned,
    };
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

//See struct ssa_block's use_before_assignment field definition in ssa_block.h
static struct u8_set calculate_use_before_assignment_set(
        struct ssa_data_node * ssa_data_node,
        struct u8_set prev_outputs
) {
    struct u8_set used_variables = get_used_locals(ssa_data_node);
    difference_u8_set(&used_variables, prev_outputs);
    return used_variables;
}

static bool is_loop_body(struct ssa_block * parent_block, parent_next_type_t type_parent_next_block) {
    if(parent_block->first->type == SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP){
        bool is_parent_loop_condition = ((struct ssa_control_conditional_jump_node *) parent_block->first)->loop_condition;
        bool is_pending_block_true_branch = type_parent_next_block == PARENT_NEXT_TYPE_TRUE_BRANCH;
        return is_parent_loop_condition && is_pending_block_true_branch;
    } else {
        return false;
    }
}