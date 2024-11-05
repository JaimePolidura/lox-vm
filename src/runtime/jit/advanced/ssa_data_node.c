#include "ssa_data_node.h"

void * allocate_ssa_data_node(ssa_data_node_type type, size_t struct_size_bytes) {
    struct ssa_data_node * ssa_control_node = malloc(struct_size_bytes);
    memset(ssa_control_node, 0, struct_size_bytes);
    ssa_control_node->type = type;
    return ssa_control_node;
}

ssa_data_type get_produced_type_ssa_data(struct ssa_data_node * start_node) {
    //TODO
    return SSA_DATA_TYPE_ANY;
}