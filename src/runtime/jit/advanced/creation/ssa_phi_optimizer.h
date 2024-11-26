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

struct phi_optimization_result optimize_ssa_ir_phis(
        struct ssa_block * start_block,
        struct phi_insertion_result * phi_insertion_result,
        struct arena_lox_allocator * ssa_nodes_allocator
);