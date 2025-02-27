#pragma once

#include "runtime/jit/advanced/lox_ir_block.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/u64_set.h"
#include "shared/utils/memory/arena.h"

struct phi_insertion_result {
    //Key: struct ssa_name, value: lox_ir_control_node
    struct u64_hash_table ssa_definitions_by_ssa_name;
    struct u8_hash_table max_version_allocated_per_local;
};

//Inserts phi functions. Replaces nodes lox_ir_control_set_local_node with lox_ir_control_define_ssa_name_node and
//lox_ir_data_get_local_node with lox_ir_data_phi_node. These phase will add innecesary phi functions like: a1 = phi(a0)
struct phi_insertion_result insert_lox_ir_phis(struct lox_ir_block *, struct arena_lox_allocator *);