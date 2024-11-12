#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"

struct pending_evaluate {
    struct ssa_block * pending_block_to_evaluate;
    struct u8_hash_table * parent_versions;
};

static void push_pending_evaluate(struct stack_list *, struct ssa_block *, struct u8_hash_table * parent_versions);
static void insert_phis_in_block(struct ssa_block *block, struct u8_hash_table * max_version_allocated_per_local, struct u8_hash_table * parent_versions);
static void insert_ssa_versions_in_control_node(struct ssa_control_node * node, struct u8_hash_table * max_version_allocated_per_local, struct u8_hash_table * parent_versions);
static void insert_phis_in_data_node(struct ssa_data_node * node, struct u8_hash_table * parent_versions);
static int allocate_new_version(struct u8_hash_table * max_version_allocated_per_local, uint8_t local_number);
static int get_version(struct u8_hash_table * parent_versions, uint8_t local_number);

void insert_ssa_ir_phis(
        struct ssa_block * start_block
) {
    start_block = start_block->next.next;

    struct u8_hash_table max_version_allocated_per_local;
    struct u64_hash_table loops_evaluted; //Stores ssa_block->next.loop address
    struct stack_list pending_evaluate_stack;
    init_u8_hash_table(&max_version_allocated_per_local);
    init_stack_list(&pending_evaluate_stack);
    init_u64_hash_table(&loops_evaluted);

    push_pending_evaluate(&pending_evaluate_stack, start_block, alloc_u8_hash_table());

    while(!is_empty_stack_list(&pending_evaluate_stack)) {
        struct pending_evaluate * pending_evaluate = pop_stack_list(&pending_evaluate_stack);
        struct ssa_block * block_to_evaluate = pending_evaluate->pending_block_to_evaluate;
        struct u8_hash_table * parent_versions = pending_evaluate->parent_versions;
        free(pending_evaluate);

        insert_phis_in_block(block_to_evaluate, &max_version_allocated_per_local, parent_versions);

        switch (block_to_evaluate->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ:
                push_pending_evaluate(&pending_evaluate_stack, block_to_evaluate->next.next, parent_versions);
                break;
            case TYPE_NEXT_SSA_BLOCK_LOOP:
                struct ssa_control_node * to_jump_loop = block_to_evaluate->next.loop;
                if(!contains_u64_hash_table(&loops_evaluted, (uint64_t) block_to_evaluate->next.loop)){
                    push_pending_evaluate(&pending_evaluate_stack, block_to_evaluate->next.loop, parent_versions);
                    put_u64_hash_table(&loops_evaluted, (uint64_t) block_to_evaluate->next.loop, (void*)0x01);
                } else {

                }
                break;
            case TYPE_NEXT_SSA_BLOCK_BRANCH:
                push_pending_evaluate(&pending_evaluate_stack, block_to_evaluate->next.branch.false_branch, clone_u8_hash_table(parent_versions));
                push_pending_evaluate(&pending_evaluate_stack, block_to_evaluate->next.branch.true_branch, parent_versions);
                break;
            case TYPE_NEXT_SSA_BLOCK_NONE:
                free(parent_versions);
                break;
        }
    }

    free_stack_list(&pending_evaluate_stack);
    free_u64_hash_table(&loops_evaluted);
}

static void insert_phis_in_block(
        struct ssa_block * block,
        struct u8_hash_table * max_version_allocated_per_local,
        struct u8_hash_table * parent_versions
) {
    if(size_u8_set(block->inputs) == 0 && size_u8_set(block->outputs) == 0){
        return;
    }

    for(struct ssa_control_node * current = block->first;; current = current->next.next){
        insert_ssa_versions_in_control_node(
                current,
                max_version_allocated_per_local,
                parent_versions
        );
    }
}

static void insert_ssa_versions_in_control_node(
        struct ssa_control_node * node,
        struct u8_hash_table * max_version_allocated_per_local,
        struct u8_hash_table * parent_versions
) {
    switch (node->type) {
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL: {
            struct ssa_control_set_local_node * set_local = (struct ssa_control_set_local_node *) node;
            uint8_t local_number = (uint8_t) set_local->local_number;
            insert_phis_in_data_node(set_local->new_local_value, parent_versions);
            int new_version = allocate_new_version(max_version_allocated_per_local, local_number);
            put_u8_hash_table(parent_versions, local_number, (void *) new_version);
            set_local->version = new_version;
            break;
        }
        case SSA_CONTROL_NODE_TYPE_DATA: {
            struct ssa_control_data_node * data_node = (struct ssa_control_data_node *) node;
            insert_phis_in_data_node(data_node->data, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) node;
            insert_phis_in_data_node(return_node->data, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) node;
            insert_phis_in_data_node(print_node->data, parent_versions);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL: {
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) node;
            insert_phis_in_data_node(set_global->value_node, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) node;
            insert_phis_in_data_node(set_struct_field->field_value, parent_versions);
            insert_phis_in_data_node(set_struct_field->instance, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array_element = (struct ssa_control_set_array_element_node *) node;
            insert_phis_in_data_node(set_array_element->new_element, parent_versions);
            insert_phis_in_data_node(set_array_element->array, parent_versions);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        case SSA_CONTROL_NODE_TYPE_START:
            break;
    }
}

static void insert_phis_in_data_node(struct ssa_data_node * node, struct u8_hash_table * parent_versions) {
    switch (node->type) {
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) node;
            uint8_t local_version = get_version(parent_versions, get_local->local_number);
            add_u8_set(&get_local->phi_versions, local_version);
            break;
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_control_function_call_node * call_node = (struct ssa_control_function_call_node *) node;
            insert_phis_in_data_node(call_node->function, parent_versions);
            for(int i = 0; i < call_node->n_arguments; i++){
                insert_phis_in_data_node(call_node->arguments[i], parent_versions);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_COMPARATION: {
            struct ssa_data_comparation_node * comparation_node = (struct ssa_data_comparation_node *) node;
            insert_phis_in_data_node(comparation_node->left, parent_versions);
            insert_phis_in_data_node(comparation_node->right, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_ARITHMETIC: {
            struct ssa_data_arithmetic_node * arithmetic_node = (struct ssa_data_arithmetic_node *) node;
            insert_phis_in_data_node(arithmetic_node->left, parent_versions);
            insert_phis_in_data_node(arithmetic_node->right, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) node;
            insert_phis_in_data_node(unary_node->unary_value_node, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field_node = (struct ssa_data_get_struct_field_node *) node;
            insert_phis_in_data_node(get_struct_field_node->instance_node, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * initialize_struct_node = (struct ssa_data_initialize_struct_node *) node;
            for(int i = 0; i < initialize_struct_node->definition->n_fields; i++){
                insert_phis_in_data_node(initialize_struct_node->fields_nodes[i], parent_versions);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element_node = (struct ssa_data_get_array_element_node *) node;
            insert_phis_in_data_node(get_array_element_node->instance, parent_versions);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * initialize_array_node = (struct ssa_data_initialize_array_node *) node;
            for(int i = 0; i < initialize_array_node->n_elements && !initialize_array_node->empty_initialization; i++){
                insert_phis_in_data_node(initialize_array_node->elememnts_node[i], parent_versions);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CONSTANT:
            break;
    }
}

static int allocate_new_version(struct u8_hash_table * max_version_allocated_per_local, uint8_t local_number) {
    int new_version = 0;

    if(contains_u8_hash_table(max_version_allocated_per_local, local_number)){
        int old_version = (int) get_u8_hash_table(max_version_allocated_per_local, local_number);
        new_version = old_version + 1;
    }

    put_u8_hash_table(max_version_allocated_per_local, local_number, (void *) new_version);

    return new_version;
}

static int get_version(struct u8_hash_table * parent_versions, uint8_t local_number) {
    if(contains_u8_hash_table(parent_versions, local_number)){
        return get_version(parent_versions, local_number);
    } else {
        put_u8_hash_table(parent_versions, local_number, (void *) 0);
        return 0;
    }
}

static void push_pending_evaluate(
        struct stack_list * pending_evaluate_stack,
        struct ssa_block * parent,
        struct u8_hash_table * parent_versions
) {
    struct pending_evaluate * pending_evaluate = malloc(sizeof(struct pending_evaluate));
    pending_evaluate->parent_versions = parent_versions;
    pending_evaluate->pending_block_to_evaluate = parent;
    push_stack_list(pending_evaluate_stack, pending_evaluate);
}