#pragma once

#include "runtime/jit/advanced/creation/no_phis_creator.h"
#include "runtime/jit/advanced/creation/phi_optimizer.h"
#include "runtime/jit/advanced/creation/phi_inserter.h"
#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/lox_ir.h"
#include "runtime/jit/advanced/lox_ir_block.h"

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/package.h"

//Given a byetcode_list this function will create the lox ir graph ir. The graph IR consists of:
//  - Data flow graph (expressions) Represented by lox_ir_data_node.
//  - Control flow graph (statements) Represented by lox_ir_control_node.
//  - Block graph. Series of control flow graph nodes, that are run sequentally. Represented by lox_ir_block
// This proccess consists of these phases:
// - 1º: create_ssa_ir_no_phis()
// - 3º: insert_ssa_ir_phis()
// - 4º: optimize_ssa_ir_phis()
struct lox_ir create_lox_ir(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode,
        long options
);
