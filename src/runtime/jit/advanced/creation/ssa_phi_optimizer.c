#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/u64_set.h"

static void remove_inncesary_phi_functions_consumer (
        struct ssa_data_node * __,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * _
);
static void remove_inncesary_phi_function(struct ssa_data_phi_node * phi_node, void ** parent_child_ptr);

//innecesary phi functions. Like a1 = phi(a0), it will replace it with: a1 = a0. a0 will be represented with the node
//ssa_data_get_ssa_name_node.
void optimize_ssa_ir_phis(
        struct ssa_block * start_block
) {
    start_block = start_block->next.next;
    struct stack_list pending;
    init_stack_list(&pending);
    push_stack_list(&pending, start_block);

    while(!is_empty_stack_list(&pending)) {
        struct ssa_block * current_block = pop_stack_list(&pending);

        for(struct ssa_control_node * current = current_block->first;; current = current->next.next) {
            for_each_data_node_in_control_node(current, NULL, remove_inncesary_phi_functions_consumer);

            if(current == current_block->last){
                break;
            }
        }

        switch(current_block->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ:
                push_stack_list(&pending, current_block->next.next);
                break;

            case TYPE_NEXT_SSA_BLOCK_BRANCH:
                push_stack_list(&pending, current_block->next.branch.false_branch);
                push_stack_list(&pending, current_block->next.branch.true_branch);
                break;

            case TYPE_NEXT_SSA_BLOCK_LOOP:
            case TYPE_NEXT_SSA_BLOCK_NONE:
                break;
        }
    }

    free_stack_list(&pending);
}

static void remove_inncesary_phi_functions_consumer(
        struct ssa_data_node * __,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * _
) {
    if (current_node->type == SSA_DATA_NODE_TYPE_PHI) {
        struct ssa_data_phi_node * phi_node = (struct ssa_data_phi_node *) current_node;
        if (size_u64_set(phi_node->ssa_definitions) == 1) {
            remove_inncesary_phi_function(phi_node, parent_child_ptr);
        }
    }
}

static void remove_inncesary_phi_function(struct ssa_data_phi_node * phi_node, void ** parent_child_ptr) {
    struct u64_set_iterator ssa_definition_iterator;
    init_u64_set_iterator(&ssa_definition_iterator, phi_node->ssa_definitions);
    void * ssa_definition_ptr = (void *) next_u64_set_iterator(&ssa_definition_iterator);
    struct ssa_control_define_ssa_name_node * ssa_definition_node = ssa_definition_ptr;

    struct ssa_data_get_ssa_name_node * new_get_ssa_name = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL
    );
    new_get_ssa_name->ssa_name = ssa_definition_node->ssa_name;

    //Replace node
    *parent_child_ptr = (struct ssa_data_node *) new_get_ssa_name;
}