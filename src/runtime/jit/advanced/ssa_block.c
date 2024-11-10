#include "ssa_block.h"

struct ssa_block * alloc_ssa_block() {
    struct ssa_block * block = malloc(sizeof(struct ssa_block));
    init_ssa_block(block);
    return block;
}

void init_ssa_block(struct ssa_block * block) {
    memset(block, 0, sizeof(struct ssa_block));
}

type_next_ssa_block_t get_type_next_ssa_block(struct ssa_control_node * node) {
    switch(node->type) {
        case SSA_CONTROL_NODE_TYPE_START:
        case SSA_CONTROL_NODE_TYPE_PRINT:
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT:
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD:
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