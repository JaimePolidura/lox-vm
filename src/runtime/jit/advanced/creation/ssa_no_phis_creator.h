#pragma once

#include "runtime/jit/advanced/ssa_block.h"
#include "runtime/jit/advanced/utils/types.h"

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/memory/arena.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/package.h"

//Creates the ssa ir, without phi functions. OP_GET_LOCAL and OP_SET_LOCAL are represented with
//ssa_control_set_local_node and ssa_data_get_local_node.
//It also creates blocks, which is a series of ssa_control_nodes that are executed sequentially without branches
struct ssa_block * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode,
        struct arena_lox_allocator * ssa_node_allocator
);