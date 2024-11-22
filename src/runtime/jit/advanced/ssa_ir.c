#include "ssa_ir.h"

static struct u64_set get_blocks_to_remove(struct ssa_block *);
static void remove_references(struct ssa_ir *, struct ssa_block *);

void remove_branch_ssa_ir(struct ssa_ir * ssa_ir, struct ssa_block * branch_block, bool true_branch) {
    struct ssa_block * branch_to_be_removed = true_branch ? branch_block->next_as.branch.true_branch : branch_block->next_as.branch.false_branch;
    struct ssa_block * branch_remains = true_branch ? branch_block->next_as.branch.false_branch : branch_block->next_as.branch.true_branch;
    if(branch_to_be_removed == NULL){
        return;
    }

    branch_block->type_next_ssa_block = TYPE_NEXT_SSA_BLOCK_SEQ;
    branch_block->next_as.next = branch_remains;

    struct u64_set blocks_to_remove = get_blocks_to_remove(branch_block);
    struct u64_set_iterator blocks_to_remove_it;
    init_u64_set_iterator(&blocks_to_remove_it, blocks_to_remove);

    while(has_next_u64_set_iterator(blocks_to_remove_it)){
        struct ssa_block * block_to_remove = (struct ssa_block *) next_u64_set_iterator(&blocks_to_remove_it);
        remove_references(ssa_ir, block_to_remove);
        free_ssa_block(block_to_remove);
    }

    free_u64_set(&blocks_to_remove);
}

//We get the set of blocks that we can remove, when we want to remove a branch. For example given the graph:
//Nodes: {A, B, C, D, E} Edges: A -> [B, C], B -> D, D -> E, C -> E
//If we concluide that we want to remove the branch A -> B. The final graph would be:
//Nodes: {A, C, E} Edges: A -> C, C -> E
static struct u64_set get_blocks_to_remove(struct ssa_block * start_block) {
    struct u64_set blocks_to_be_removed;
    init_u64_set(&blocks_to_be_removed);
    add_u64_set(&blocks_to_be_removed, (uint64_t) start_block);

    struct queue_list pending;
    init_queue_list(&pending);

    enqueue_queue_list(&pending, start_block);

    while(!is_empty_queue_list(&pending)) {
        struct ssa_block * current_block = dequeue_queue_list(&pending);
        struct u64_set_iterator current_block_predecesors_it;
        init_u64_set_iterator(&current_block_predecesors_it, current_block->predecesors);

        //We check that the predecessors of the node that we are scanning, is contained the set of nodes that we want to
        //remove
        bool current_block_can_be_removed = true;
        while(has_next_u64_set_iterator(current_block_predecesors_it)){
            uint64_t current_block_predecesor_ptr = next_u64_set_iterator(&current_block_predecesors_it);
            struct ssa_block * current_block_predecesor = (struct ssa_block *) current_block_predecesor_ptr;

            if(!contains_u64_set(&blocks_to_be_removed, current_block_predecesor_ptr)){
                current_block_can_be_removed = false;
                break;
            }
        }

        if (current_block_can_be_removed) {
            add_u64_set(&blocks_to_be_removed, (uint64_t) current_block);

            if(current_block->type_next_ssa_block == TYPE_NEXT_SSA_BLOCK_SEQ){
                enqueue_queue_list(&pending, current_block->next_as.next);
            } else if(current_block->type_next_ssa_block == TYPE_NEXT_SSA_BLOCK_BRANCH) {
                enqueue_queue_list(&pending, current_block->next_as.branch.false_branch);
                enqueue_queue_list(&pending, current_block->next_as.branch.true_branch);
            }
        }
    }

    free_queue_list(&pending);

    return blocks_to_be_removed;
}

//We will remove any reference in the ssa_ir to ssa_block
static void remove_references(struct ssa_ir * ssa_ir, struct ssa_block * ssa_block_to_remove) {
    struct u64_set_iterator defined_ssa_names_it;
    init_u64_set_iterator(&defined_ssa_names_it, ssa_block_to_remove->defined_ssa_names);

    while(has_next_u64_set_iterator(defined_ssa_names_it)){
        struct ssa_name defined_ssa_name_in_block = CREATE_SSA_NAME_FROM_U64(next_u64_set_iterator(&defined_ssa_names_it));
        //TODO
    }
}