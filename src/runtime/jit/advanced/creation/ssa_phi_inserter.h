#pragma once

#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/u64_set.h"

struct phi_insertion_result {
    //Key: struct ssa_name, value: ssa_control_node
    struct u64_hash_table ssa_definitions_by_ssa_name;
};

struct phi_insertion_result insert_ssa_ir_phis(struct ssa_block * start_block);