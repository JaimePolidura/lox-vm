#pragma once

#include "runtime/jit/advanced/creation/phi_inserter.h"
#include "runtime/jit/advanced/lox_ir_block.h"
#include "runtime/jit/advanced/lox_ir.h"

#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/u64_set.h"

struct phi_optimization_result {
    //u64_set of control_nodes per jit name
    struct u64_hash_table node_uses_by_ssa_name;
};

//Removes innecesary phi functions. Like a1 = phi(a0), it will replace it with: a1 = a0. a0 will be represented with the control_node_to_lower
//Also extract phi nodes to jit names, for example: print phi(a0, a1) -> a2 = phi(a0, a1); print a2
void optimize_lox_ir_phis(struct lox_ir *);