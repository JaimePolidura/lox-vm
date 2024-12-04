#include "ssa_block.h"

struct ssa_block * alloc_ssa_block(struct lox_allocator * allocator) {
    struct ssa_block * block = LOX_MALLOC(allocator, sizeof(struct ssa_block));
    init_ssa_block(block, allocator);
    return block;
}

void init_ssa_block(struct ssa_block * block, struct lox_allocator * allocator) {
    memset(block, 0, sizeof(struct ssa_block));
    init_u64_set(&block->predecesors, allocator);
    init_u64_set(&block->defined_ssa_names, allocator);
}

type_next_ssa_block_t get_type_next_ssa_block(struct ssa_control_node * node) {
    switch(node->type) {
        case SSA_CONTROL_NODE_TYPE_PRINT:
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT:
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL:
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD:
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME:
        case SSA_CONTROL_NODE_TYPE_DATA: {
            return TYPE_NEXT_SSA_BLOCK_SEQ;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            return TYPE_NEXT_SSA_BLOCK_NONE;
        }
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP: {
            return TYPE_NEXT_SSA_BLOCK_LOOP;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP: {
            return TYPE_NEXT_SSA_BLOCK_BRANCH;
        }
    }
}

void remove_control_node_ssa_block(
        struct ssa_block * ssa_block,
        struct ssa_control_node * node_to_remove
) {
    //The block has only 1 control node
    if(ssa_block->first == ssa_block->last){
        ssa_block->first = ssa_block->last = NULL;
        return;
    }
    //We are removing the first node
    if(ssa_block->first == node_to_remove) {
        ssa_block->first = node_to_remove->next;
    }
    //We are removing the last node
    if(ssa_block->last == node_to_remove){
        ssa_block->last = node_to_remove->prev;
    }
    //There is only one node remaining
    if(ssa_block->last == ssa_block->first){
        ssa_block->first->next = NULL;
        ssa_block->first->prev = NULL;
        return;
    }
    //Unlink the node from the control node linkedlist in a block
    if (node_to_remove != NULL) {
        node_to_remove->prev->next = node_to_remove->next;
    }
    if(node_to_remove->next != NULL){
        node_to_remove->next->prev = node_to_remove->prev;
    }
}

void add_last_control_node_ssa_block(struct ssa_block * block, struct ssa_control_node * node) {
    if(block->first == NULL){
        block->first = node;
    }
    if(block->last != NULL) {
        block->last->next = node;
        node->prev = block->last;
    }

    block->last = node;
}


void add_before_control_node_ssa_block(
        struct ssa_block * block,
        struct ssa_control_node * before,
        struct ssa_control_node * new
) {
    if(block->first == before){
        block->first = new;
    }

    if(before->prev != NULL){
        new->prev = before->prev;
        before->prev->next = new;
    }

    new->next = before;
    before->prev = new;
}

static struct u64_set get_blocks_to_remove(struct ssa_block *);

bool is_emtpy_ssa_block(struct ssa_block * block) {
    return block->first == NULL && block->last == NULL;
}

struct branch_removed remove_branch_ssa_ir(
        struct ssa_block * branch_block,
        bool true_branch,
        struct lox_allocator * lox_allocator
) {
    struct ssa_block * branch_to_be_removed = true_branch ? branch_block->next_as.branch.true_branch : branch_block->next_as.branch.false_branch;
    struct ssa_block * branch_remains = true_branch ? branch_block->next_as.branch.false_branch : branch_block->next_as.branch.true_branch;
    if (branch_to_be_removed == NULL) {
        return (struct branch_removed) {
            .ssa_name_definitions_removed = empty_u64_set(lox_allocator),
            .blocks_removed = empty_u64_set(lox_allocator),
        };
    }

    struct u64_set blocks_removed = get_blocks_to_remove(branch_to_be_removed);

    //We get the set of ssa names removed
    struct u64_set_iterator subgraph_blocks_to_remove_it;
    init_u64_set_iterator(&subgraph_blocks_to_remove_it, blocks_removed);
    struct u64_set ssa_name_definitinos_subgraph;
    init_u64_set(&ssa_name_definitinos_subgraph, lox_allocator);
    while (has_next_u64_set_iterator(subgraph_blocks_to_remove_it)) {
        struct ssa_block * block_to_remove = (struct ssa_block *) next_u64_set_iterator(&subgraph_blocks_to_remove_it);
        union_u64_set(&ssa_name_definitinos_subgraph, block_to_remove->defined_ssa_names);
    }

    branch_block->type_next_ssa_block = TYPE_NEXT_SSA_BLOCK_SEQ;
    branch_block->next_as.next = branch_remains;
    remove_control_node_ssa_block(branch_block, branch_block->last); //The last is the jump_conditional node
    if(is_emtpy_ssa_block(branch_block)){
        add_u64_set(&blocks_removed, (uint64_t) branch_block);
        replace_block_ssa_block(branch_block, branch_remains);
    }

    return (struct branch_removed) {
        .ssa_name_definitions_removed = ssa_name_definitinos_subgraph,
        .blocks_removed = blocks_removed,
    };
}

//We get the set of blocks that we can remove, when we want to remove a branch. For example given the graph:
//Nodes: {A, B, C, D, E} Edges: A -> [B, C], B -> D, D -> E, C -> E
//If we concluide that we want to remove the branch A -> B. The final graph would be:
//Nodes: {A, C, E} Edges: A -> C, C -> E
static struct u64_set get_blocks_to_remove(struct ssa_block * start_block) {
    if(size_u64_set(start_block->predecesors) > 1){
        return empty_u64_set(NATIVE_LOX_ALLOCATOR());
    }

    struct u64_set blocks_to_be_removed;
    init_u64_set(&blocks_to_be_removed, NATIVE_LOX_ALLOCATOR());
    add_u64_set(&blocks_to_be_removed, (uint64_t) start_block);

    struct queue_list pending;
    init_queue_list(&pending, NATIVE_LOX_ALLOCATOR());

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

void replace_block_ssa_block(struct ssa_block * old_block, struct ssa_block * new_block) {
    FOR_EACH_U64_SET_VALUE(old_block->predecesors, predecesor_u64_ptr) {
        struct ssa_block * predecesor_node = (void *) predecesor_u64_ptr;
        switch (predecesor_node->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_LOOP: {
                if (predecesor_node->next_as.loop == old_block) {
                    predecesor_node->next_as.loop = new_block;
                }
                break;
            }
            case TYPE_NEXT_SSA_BLOCK_SEQ: {
                if(predecesor_node->next_as.next == old_block) {
                    predecesor_node->next_as.next = new_block;
                }
                break;
            }
            case TYPE_NEXT_SSA_BLOCK_BRANCH: {
                if(predecesor_node->next_as.branch.true_branch == old_block){
                    predecesor_node->next_as.branch.true_branch = new_block;
                }
                if(predecesor_node->next_as.branch.false_branch == old_block){
                    predecesor_node->next_as.branch.false_branch = new_block;
                }
                break;
            }
            case TYPE_NEXT_SSA_BLOCK_NONE:
                break;
        }
    }
}