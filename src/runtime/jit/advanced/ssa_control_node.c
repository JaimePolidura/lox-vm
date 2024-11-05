#include "ssa_control_node.h"

void * allocate_ssa_block_node(ssa_control_node_type type, size_t struct_size_bytes) {
    struct ssa_control_node * ssa_control_node = malloc(struct_size_bytes);
    memset(ssa_control_node, 0, struct_size_bytes);
    ssa_control_node->type = type;
    return ssa_control_node;
}