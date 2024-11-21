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

struct ssa_creation_result {
    struct ssa_block * start_block;
    //SSA Definitions nodes by ssa name
    struct u64_hash_table ssa_definitions_by_ssa_name;
    //Max version allocated per local number
    struct u8_hash_table max_version_allocated_per_local;
    //u64_set of ssa_control_nodes per ssa name
    struct u64_hash_table node_uses_by_ssa_name;
};

//Given a byetcode_list this function will create the ssa graph ir. The graph IR consists of:
//  - Data flow graph (expressions) Represented by ssa_data_node.
//  - Control flow graph (statements) Represented by ssa_control_node.
//  - Block graph. Series of control flow graph nodes, that are run sequentally. Represented by ssa_block
// This proccess consists of these phases:
// - 1º: create_ssa_ir_no_phis()
// - 3º: insert_ssa_ir_phis()
// - 4º: optimize_ssa_ir_phis()
struct ssa_creation_result create_ssa_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
);
