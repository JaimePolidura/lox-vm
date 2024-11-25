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