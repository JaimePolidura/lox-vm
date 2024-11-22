#pragma once

#include "runtime/jit/advanced/creation/ssa_phi_optimizer.h"
#include "runtime/jit/advanced/creation/ssa_phi_inserter.h"
#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/ssa_block.h"

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/package.h"

struct ssa_ir {
    struct ssa_block * first_block;
    //SSA Definitions nodes by ssa name
    struct u64_hash_table ssa_definitions_by_ssa_name;
    //Max version allocated per local number
    struct u8_hash_table max_version_allocated_per_local;
    //u64_hash_table of ssa_control_nodes per ssa name
    struct u64_hash_table node_uses_by_ssa_name;
};

void remove_branch_ssa_ir(struct ssa_ir *, struct ssa_block *, bool true_branch);