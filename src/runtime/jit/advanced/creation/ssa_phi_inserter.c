#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/u64_set.h"

struct ssa_phi_inserter {
    struct u8_hash_table max_version_allocated_per_local;
    struct u64_set loops_evaluted; //Stores ssa_block->next.loop address
    struct u64_set extracted_variables_loop_body;
    struct stack_list pending_evaluate;
};

struct pending_evaluate {
    struct ssa_block * pending_block_to_evaluate;
    struct u8_hash_table * parent_versions;
};

static void push_pending_evaluate(struct ssa_phi_inserter *, struct ssa_block *, struct u8_hash_table * parent_versions);
static void insert_phis_in_block(struct ssa_phi_inserter *, struct ssa_block *block, struct u8_hash_table * parent_versions);
static void insert_ssa_versions_in_control_node(struct ssa_phi_inserter *, struct ssa_block *, struct ssa_control_node * control_node, struct u8_hash_table * parent_versions);
static void insert_phis_in_data_node(struct ssa_phi_inserter *, struct ssa_block *, struct ssa_control_node *, struct ssa_data_node *, struct u8_hash_table * parent_versions);
static int allocate_new_version(struct u8_hash_table * max_version_allocated_per_local, uint8_t local_number);
static int get_version(struct u8_hash_table * parent_versions, uint8_t local_number);
static struct ssa_phi_inserter * alloc_ssa_phi_inserter();
static void free_ssa_phi_inserter(struct ssa_phi_inserter *);
static void extract_get_local(struct ssa_phi_inserter * inserter, struct u8_hash_table * parent_versions, struct ssa_control_node * control_node_to_extract,
        struct ssa_data_get_local_node * get_local_node_to_extract, struct ssa_block *, uint8_t local_number);

void insert_ssa_ir_phis(
        struct ssa_block * start_block
) {
    start_block = start_block->next.next;

    struct ssa_phi_inserter * inserter = alloc_ssa_phi_inserter();

    push_pending_evaluate(inserter, start_block, alloc_u8_hash_table());

    while(!is_empty_stack_list(&inserter->pending_evaluate)) {
        struct pending_evaluate * pending_evaluate = pop_stack_list(&inserter->pending_evaluate);
        struct ssa_block * block_to_evaluate = pending_evaluate->pending_block_to_evaluate;
        struct u8_hash_table * parent_versions = pending_evaluate->parent_versions;
        free(pending_evaluate);

        insert_phis_in_block(inserter, block_to_evaluate, parent_versions);

        switch (block_to_evaluate->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ:
                push_pending_evaluate(inserter, block_to_evaluate->next.next, parent_versions);
                break;
            case TYPE_NEXT_SSA_BLOCK_LOOP:
                struct ssa_block * to_jump_loop_block = block_to_evaluate->next.loop;
                if(!contains_u64_set(&inserter->loops_evaluted, (uint64_t) to_jump_loop_block)){
                    push_pending_evaluate(inserter, to_jump_loop_block, parent_versions);
                    add_u64_set(&inserter->loops_evaluted, (uint64_t) to_jump_loop_block);
                } else {
                    //OP_LOOP always points to the loop condition
                    //If an assigment have ocurred inside the loop body, we will need to propagate outside the loop
                    push_pending_evaluate(inserter, to_jump_loop_block->next.branch.false_branch, parent_versions);
                }
                break;
            case TYPE_NEXT_SSA_BLOCK_BRANCH:
                push_pending_evaluate(inserter, block_to_evaluate->next.branch.false_branch, clone_u8_hash_table(parent_versions));
                push_pending_evaluate(inserter, block_to_evaluate->next.branch.true_branch, parent_versions);
                break;
            case TYPE_NEXT_SSA_BLOCK_NONE:
                free(parent_versions);
                break;
        }
    }

    free_ssa_phi_inserter(inserter);
}

static void insert_phis_in_block(
        struct ssa_phi_inserter * inserter,
        struct ssa_block * block,
        struct u8_hash_table * parent_versions
) {
    for(struct ssa_control_node * current = block->first;; current = current->next.next){
        insert_ssa_versions_in_control_node(inserter, block, current, parent_versions);

        if(current == block->last){
            break;
        }
    }
}

static void insert_ssa_versions_in_control_node(
        struct ssa_phi_inserter * inserter,
        struct ssa_block * block,
        struct ssa_control_node * control_node,
        struct u8_hash_table * parent_versions
) {
    switch (control_node->type) {
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL: {
            struct ssa_control_set_local_node * set_local = (struct ssa_control_set_local_node *) control_node;
            uint8_t local_number = (uint8_t) set_local->local_number;
            insert_phis_in_data_node(inserter, block, control_node, set_local->new_local_value, parent_versions);
            //Version not assigned
            if(set_local->version == 0){
                int new_version = allocate_new_version(&inserter->max_version_allocated_per_local, local_number);
                put_u8_hash_table(parent_versions, local_number, (void *) new_version);
                set_local->version = new_version;
            }

            break;
        }
        case SSA_CONTROL_NODE_TYPE_DATA: {
            struct ssa_control_data_node * data_node = (struct ssa_control_data_node *) control_node;
            insert_phis_in_data_node(inserter, block, control_node, data_node->data, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) control_node;
            insert_phis_in_data_node(inserter, block, control_node, return_node->data, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) control_node;
            insert_phis_in_data_node(inserter, block, control_node, print_node->data, parent_versions);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL: {
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) control_node;
            insert_phis_in_data_node(inserter, block, control_node, set_global->value_node, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) control_node;
            insert_phis_in_data_node(inserter, block, control_node, set_struct_field->field_value, parent_versions);
            insert_phis_in_data_node(inserter, block, control_node, set_struct_field->instance, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array_element = (struct ssa_control_set_array_element_node *) control_node;
            insert_phis_in_data_node(inserter, block, control_node, set_array_element->new_element, parent_versions);
            insert_phis_in_data_node(inserter, block, control_node, set_array_element->array, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
            struct ssa_control_conditional_jump_node * conditional_jump = (struct ssa_control_conditional_jump_node *) control_node;
            insert_phis_in_data_node(inserter, block, control_node, conditional_jump->condition, parent_versions);
            break;
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        case SSA_CONTROL_NODE_TYPE_START:
            break;
    }
}

static void insert_phis_in_data_node_recursive(
        struct ssa_phi_inserter * inserter,
        int expression_deep_level,
        struct ssa_block * block,
        struct ssa_control_node * control_node,
        struct ssa_data_node * data_node,
        struct u8_hash_table * parent_versions
) {
    switch (data_node->type) {
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) data_node;
            uint8_t local_number = get_local->local_number;

            if(block->loop_body &&
                    expression_deep_level > 0 &&
                    contains_u8_set(&block->use_before_assigment, local_number) &&
                    !contains_u64_set(&inserter->extracted_variables_loop_body, (uint64_t) control_node)
            ){
                extract_get_local(inserter, parent_versions, control_node, get_local, block, local_number);
            } else if (!contains_u64_set(&inserter->extracted_variables_loop_body, (uint64_t) control_node)) {
                uint8_t local_version = get_version(parent_versions, get_local->local_number);
                add_u8_set(&get_local->phi_versions, local_version);
            }

            break;
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_control_function_call_node * call_node = (struct ssa_control_function_call_node *) data_node;
            insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                               call_node->function, parent_versions);
            for(int i = 0; i < call_node->n_arguments; i++){
                insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                                   call_node->arguments[i], parent_versions);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_COMPARATION: {
            struct ssa_data_comparation_node * comparation_node = (struct ssa_data_comparation_node *) data_node;
            insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                               comparation_node->left, parent_versions);
            insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                               comparation_node->right, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_ARITHMETIC: {
            struct ssa_data_arithmetic_node * arithmetic_node = (struct ssa_data_arithmetic_node *) data_node;
            insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                               arithmetic_node->left, parent_versions);
            insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                               arithmetic_node->right, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) data_node;
            insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                               unary_node->unary_value_node, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field_node = (struct ssa_data_get_struct_field_node *) data_node;
            insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                               get_struct_field_node->instance_node, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * initialize_struct_node = (struct ssa_data_initialize_struct_node *) data_node;
            for(int i = 0; i < initialize_struct_node->definition->n_fields; i++){
                insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                                   initialize_struct_node->fields_nodes[i], parent_versions);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element_node = (struct ssa_data_get_array_element_node *) data_node;
            insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                               get_array_element_node->instance, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * initialize_array_node = (struct ssa_data_initialize_array_node *) data_node;
            for(int i = 0; i < initialize_array_node->n_elements && !initialize_array_node->empty_initialization; i++){
                insert_phis_in_data_node_recursive(inserter, expression_deep_level + 1, block, control_node,
                                                   initialize_array_node->elememnts_node[i], parent_versions);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CONSTANT:
            break;
    }
}

static void insert_phis_in_data_node(
        struct ssa_phi_inserter * inserter,
        struct ssa_block * block,
        struct ssa_control_node * control_node,
        struct ssa_data_node * data_node,
        struct u8_hash_table * parent_versions
) {
    //Used for correct generation in loop bodies
    insert_phis_in_data_node_recursive(
            inserter,
            0,
            block,
            control_node,
            data_node,
            parent_versions
    );
}

static int allocate_new_version(struct u8_hash_table * max_version_allocated_per_local, uint8_t local_number) {
    int new_version = 1;

    if(contains_u8_hash_table(max_version_allocated_per_local, local_number)){
        int old_version = (int) get_u8_hash_table(max_version_allocated_per_local, local_number);
        new_version = old_version + 1;
    }

    put_u8_hash_table(max_version_allocated_per_local, local_number, (void *) new_version);

    return new_version;
}

static int get_version(struct u8_hash_table * parent_versions, uint8_t local_number) {
    if(contains_u8_hash_table(parent_versions, local_number)){
        return (int) get_u8_hash_table(parent_versions, local_number);
    } else {
        put_u8_hash_table(parent_versions, local_number, (void *) 0);
        return 0;
    }
}

static void push_pending_evaluate(
        struct ssa_phi_inserter * inserter,
        struct ssa_block * parent,
        struct u8_hash_table * parent_versions
) {
    struct pending_evaluate * pending_evaluate = malloc(sizeof(struct pending_evaluate));
    pending_evaluate->parent_versions = parent_versions;
    pending_evaluate->pending_block_to_evaluate = parent;
    push_stack_list(&inserter->pending_evaluate, pending_evaluate);
}

static struct ssa_phi_inserter * alloc_ssa_phi_inserter() {
    struct ssa_phi_inserter * inserter = malloc(sizeof(struct ssa_phi_inserter));
    init_u8_hash_table(&inserter->max_version_allocated_per_local);
    init_u64_set(&inserter->extracted_variables_loop_body);
    init_stack_list(&inserter->pending_evaluate);
    init_u64_set(&inserter->loops_evaluted);
    return inserter;
}

static void free_ssa_phi_inserter(struct ssa_phi_inserter * inserter) {
    free_stack_list(&inserter->pending_evaluate);
    free_u64_set(&inserter->loops_evaluted);
    free(inserter);
}

static void extract_get_local(
        struct ssa_phi_inserter * inserter,
        struct u8_hash_table * parent_versions,
        struct ssa_control_node * control_node_to_extract,
        struct ssa_data_get_local_node * get_local_node_to_extract,
        struct ssa_block * to_extract_block,
        uint8_t local_number
) {
    struct ssa_control_set_local_node * extracted_set_local = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTORL_NODE_TYPE_SET_LOCAL, struct ssa_control_set_local_node
    );
    struct ssa_data_get_local_node * extracted_get_local = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_LOCAL, struct ssa_data_get_local_node, NULL
    );

    extracted_get_local->local_number = local_number;
    extracted_get_local->type = get_local_node_to_extract->type;
    struct u8_set new_get_local_versions;
    init_u8_set(&new_get_local_versions);
    add_u8_set(&new_get_local_versions, get_version(parent_versions, local_number));
    extracted_get_local->phi_versions = new_get_local_versions;

    int new_version_extracted = allocate_new_version(&inserter->max_version_allocated_per_local, local_number);
    extracted_set_local->new_local_value = (struct ssa_data_node *) extracted_get_local;
    extracted_set_local->version = new_version_extracted;
    extracted_set_local->local_number = local_number;

    get_local_node_to_extract->phi_versions = create_u8_set(1, new_version_extracted);

    extracted_set_local->control.next.next = control_node_to_extract;
    extracted_set_local->control.prev = control_node_to_extract->prev;
    control_node_to_extract->prev->next.next = &extracted_set_local->control;
    control_node_to_extract->prev = &extracted_set_local->control;
    if(to_extract_block->first == control_node_to_extract){
        to_extract_block->first = &extracted_set_local->control;
    }

    add_u64_set(&inserter->extracted_variables_loop_body, (uint64_t) control_node_to_extract);
}