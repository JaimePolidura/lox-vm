#include "runtime/jit/advanced/creation/ssa_phi_inserter.h"
#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/u64_set.h"

struct optimize_phi_functions_consumer_struct {
    struct phi_insertion_result * phi_insertion_result;
    struct ssa_control_node * control_node;
    struct ssa_block * block;
};

static void optimize_phi_functions_consumer (
        struct ssa_data_node * __,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * control_node_ptr
);
static void remove_innecesary_phi_function(struct ssa_data_phi_node * phi_node, void ** parent_child_ptr);
static void extract_phi_to_ssa_name(struct optimize_phi_functions_consumer_struct *, struct ssa_data_phi_node *, void ** parent_child_ptr);
static uint8_t allocate_new_ssa_version(int local_number, struct phi_insertion_result *);

//Removes innecesary phi functions. Like a1 = phi(a0), it will replace it with: a1 = a0. a0 will be represented with the node
//Also extract phi nodes to ssa names, for example: print phi(a0, a1) -> a2 = phi(a0, a1); print a2
void optimize_ssa_ir_phis(
        struct ssa_block * start_block,
        struct phi_insertion_result * phi_insertion_result
) {
    struct stack_list pending;
    init_stack_list(&pending);
    push_stack_list(&pending, start_block);

    while(!is_empty_stack_list(&pending)) {
        struct ssa_block * current_block = pop_stack_list(&pending);

        for(struct ssa_control_node * current = current_block->first;; current = current->next) {
            struct optimize_phi_functions_consumer_struct for_each_node_struct = (struct optimize_phi_functions_consumer_struct) {
                    .phi_insertion_result = phi_insertion_result,
                    .control_node = current,
                    .block = current_block,
            };

            for_each_data_node_in_control_node(current, &for_each_node_struct, optimize_phi_functions_consumer);

            if(current == current_block->last){
                break;
            }
        }

        switch(current_block->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ:
                push_stack_list(&pending, current_block->next_as.next);
                break;

            case TYPE_NEXT_SSA_BLOCK_BRANCH:
                push_stack_list(&pending, current_block->next_as.branch.false_branch);
                push_stack_list(&pending, current_block->next_as.branch.true_branch);
                break;

            case TYPE_NEXT_SSA_BLOCK_LOOP:
            case TYPE_NEXT_SSA_BLOCK_NONE:
                break;
        }
    }

    free_stack_list(&pending);
}

static void optimize_phi_functions_consumer(
        struct ssa_data_node * __,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * consumer_struct_ptr
) {
    struct optimize_phi_functions_consumer_struct * for_each_node_consumer_struct = consumer_struct_ptr;

    if (current_node->type == SSA_DATA_NODE_TYPE_PHI) {
        struct ssa_data_phi_node * phi_node = (struct ssa_data_phi_node *) current_node;
        if (size_u64_set(phi_node->ssa_definitions) == 1) {
            remove_innecesary_phi_function(phi_node, parent_child_ptr);
        } else if(size_u64_set(phi_node->ssa_definitions) > 1 && for_each_node_consumer_struct->control_node->type != SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME) {
            extract_phi_to_ssa_name(for_each_node_consumer_struct, (struct ssa_data_phi_node *) current_node, parent_child_ptr);
        }
    }
}

static void remove_innecesary_phi_function(
        struct ssa_data_phi_node * phi_node,
        void ** parent_child_ptr
) {
    struct u64_set_iterator ssa_definition_iterator;
    init_u64_set_iterator(&ssa_definition_iterator, phi_node->ssa_definitions);
    void * ssa_definition_ptr = (void *) next_u64_set_iterator(&ssa_definition_iterator);
    struct ssa_control_define_ssa_name_node * ssa_definition_node = ssa_definition_ptr;

    struct ssa_data_get_ssa_name_node * new_get_ssa_name = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL
    );
    new_get_ssa_name->ssa_name = ssa_definition_node->ssa_name;
    new_get_ssa_name->definition_node = ssa_definition_node;

    //Replace node
    *parent_child_ptr = (void *) new_get_ssa_name;
}

//Extracts phi functions to ssa names
//Example: Given: print phi(a0, a1). Result: a2 = phi(a0, a1); print a2;
static void extract_phi_to_ssa_name(
        struct optimize_phi_functions_consumer_struct * consumer_struct,
        struct ssa_data_phi_node * phi_node,
        void ** parent_child_ptr
) {
    struct phi_insertion_result * phi_insertion_result = consumer_struct->phi_insertion_result;
    struct ssa_control_node * control_node = consumer_struct->control_node;
    struct ssa_block * block = consumer_struct->block;

    uint8_t extracted_version = allocate_new_ssa_version(phi_node->local_number, phi_insertion_result);
    struct ssa_name extracted_ssa_name = CREATE_SSA_NAME(phi_node->local_number, extracted_version);
    struct ssa_control_define_ssa_name_node * extracted_define_ssa_name = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME, struct ssa_control_define_ssa_name_node
    );
    extracted_define_ssa_name->ssa_name = extracted_ssa_name;
    extracted_define_ssa_name->value = &phi_node->data;

    struct ssa_data_get_ssa_name_node * get_extracted = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL
    );

    get_extracted->definition_node = (void *) extracted_define_ssa_name;
    get_extracted->ssa_name = extracted_ssa_name;

    add_before_control_node_ssa_block(block, control_node, &extracted_define_ssa_name->control);
    *parent_child_ptr = get_extracted;
}

static uint8_t allocate_new_ssa_version(int local_number, struct phi_insertion_result * phi_insertion_result) {
    uint8_t last_version = (uint8_t) get_u8_hash_table(&phi_insertion_result->max_version_allocated_per_local, local_number);
    uint8_t new_version = last_version + 1;
    put_u8_hash_table(&phi_insertion_result->max_version_allocated_per_local, local_number, (void *) new_version);

    return new_version;
}