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
        case SSA_CONTROL_NODE_TYPE_START:
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

    branch_block->type_next_ssa_block = TYPE_NEXT_SSA_BLOCK_SEQ;
    branch_block->next_as.next = branch_remains;

    struct u64_set subgraph_blocks_to_remove = get_blocks_to_remove(branch_block);
    struct u64_set_iterator subgraph_blocks_to_remove_it;
    init_u64_set_iterator(&subgraph_blocks_to_remove_it, subgraph_blocks_to_remove);

    struct u64_set ssa_name_definitinos_subgraph;
    init_u64_set(&ssa_name_definitinos_subgraph, lox_allocator);

    while (has_next_u64_set_iterator(subgraph_blocks_to_remove_it)) {
        struct ssa_block * block_to_remove = (struct ssa_block *) next_u64_set_iterator(&subgraph_blocks_to_remove_it);
        union_u64_set(&ssa_name_definitinos_subgraph, block_to_remove->defined_ssa_names);
    }

    return (struct branch_removed) {
            .ssa_name_definitions_removed = ssa_name_definitinos_subgraph,
            .blocks_removed = subgraph_blocks_to_remove,
    };
}

//We get the set of blocks that we can remove, when we want to remove a branch. For example given the graph:
//Nodes: {A, B, C, D, E} Edges: A -> [B, C], B -> D, D -> E, C -> E
//If we concluide that we want to remove the branch A -> B. The final graph would be:
//Nodes: {A, C, E} Edges: A -> C, C -> E
static struct u64_set get_blocks_to_remove(struct ssa_block * start_block) {
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
