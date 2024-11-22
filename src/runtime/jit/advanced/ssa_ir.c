#include "ssa_ir.h"

void remove_branch_ssa_ir(struct ssa_ir * ssa_ir, struct ssa_block * branch_block, bool true_branch) {
    struct ssa_block * branch_to_be_removed = true_branch ? branch_block->next_as.branch.true_branch : branch_block->next_as.branch.false_branch;
    struct ssa_block * branch_remains = true_branch ? branch_block->next_as.branch.false_branch : branch_block->next_as.branch.true_branch;

    branch_block->type_next_ssa_block = TYPE_NEXT_SSA_BLOCK_SEQ;
    branch_block->next_as.next = branch_remains;

        
}