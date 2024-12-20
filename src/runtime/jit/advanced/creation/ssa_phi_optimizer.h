#pragma once

#include "runtime/jit/advanced/creation/ssa_phi_inserter.h"
#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/u64_set.h"

struct phi_optimization_result {
    //u64_set of ssa_control_nodes per ssa name
    struct u64_hash_table node_uses_by_ssa_name;
};

//Removes innecesary phi functions. Like a1 = phi(a0), it will replace it with: a1 = a0. a0 will be represented with the control_node
//Also extract phi nodes to ssa names, for example: print phi(a0, a1) -> a2 = phi(a0, a1); print a2
struct phi_optimization_result optimize_ssa_ir_phis(
        struct ssa_block * start_block,
        struct phi_insertion_result * phi_insertion_result,
        struct arena_lox_allocator * ssa_nodes_allocator
);