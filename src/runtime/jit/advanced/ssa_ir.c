#include "ssa_ir.h"

static struct ptr_arraylist get_blocks_to_remove(struct ssa_block *);
static void remove_references(struct ssa_ir *, struct ssa_block *);

void remove_branch_ssa_ir(struct ssa_ir * ssa_ir, struct ssa_block * branch_block, bool true_branch) {
    struct ssa_block * branch_to_be_removed = true_branch ? branch_block->next_as.branch.true_branch : branch_block->next_as.branch.false_branch;
    struct ssa_block * branch_remains = true_branch ? branch_block->next_as.branch.false_branch : branch_block->next_as.branch.true_branch;
    if(branch_to_be_removed == NULL){
        return;
    }

    branch_block->type_next_ssa_block = TYPE_NEXT_SSA_BLOCK_SEQ;
    branch_block->next_as.next = branch_remains;

    struct ptr_arraylist blocks_to_remove = get_blocks_to_remove(branch_block);
    for(int i = 0; i < blocks_to_remove.in_use; i++){
        struct ssa_block * block_to_remove = blocks_to_remove.values[i];

        remove_references(ssa_ir, block_to_remove);
        free_ssa_block(block_to_remove);
    }

    free_ptr_arraylist(&blocks_to_remove);
}

static void remove_references(struct ssa_ir * ssa_ir, struct ssa_block * ssa_block) {

}

struct pending_block_to_remove {
    struct ssa_block * block;
    int expected_predecessors;
};

struct pending_block_to_remove * alloc_pending_block_to_remove(struct ssa_block * block, int expected_predecessors) {
    struct pending_block_to_remove * pending = malloc(sizeof(struct pending_block_to_remove));
    pending->expected_predecessors = expected_predecessors;
    pending->block = block;
    return pending;
}

static struct ptr_arraylist get_blocks_to_remove(struct ssa_block * start_block) {
    struct ptr_arraylist blocks_to_be_removed;
    init_ptr_arraylist(&blocks_to_be_removed);

    struct stack_list pending;
    init_stack_list(&pending);

    push_stack_list(&pending, alloc_pending_block_to_remove(start_block, size_u64_set(start_block->predecesors)));

    while(!is_empty_stack_list(&pending)) {
        struct pending_block_to_remove * pending_to_remove = pop_stack_list(&pending);
        int n_current_predecessors = size_u64_set(pending_to_remove->block->predecesors);
        struct ssa_block * current_block = pending_to_remove->block;

        if (n_current_predecessors == pending_to_remove->expected_predecessors){
            append_ptr_arraylist(&blocks_to_be_removed, pending_to_remove->block);

            if(pending_to_remove->block->type_next_ssa_block == TYPE_NEXT_SSA_BLOCK_SEQ){
                push_stack_list(&pending, alloc_pending_block_to_remove(current_block->next_as.next, 1));
            } else if(pending_to_remove->block->type_next_ssa_block == TYPE_NEXT_SSA_BLOCK_BRANCH) {
                push_stack_list(&pending, alloc_pending_block_to_remove(current_block->next_as.branch.false_branch, 1));
                push_stack_list(&pending, alloc_pending_block_to_remove(current_block->next_as.branch.true_branch, 1));
            }
        }

        free(pending_to_remove);
    }

    return blocks_to_be_removed;
}