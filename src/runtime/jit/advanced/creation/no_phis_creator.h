#pragma once

#include "runtime/jit/advanced/utils/types.h"
#include "runtime/jit/advanced/lox_ir_options.h"
#include "runtime/jit/advanced/lox_ir_block.h"

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/memory/arena.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/package.h"

//Creates the lox ir, without phi functions. OP_GET_LOCAL and OP_SET_LOCAL are represented with
//lox_ir_control_set_local_node and lox_ir_data_get_local_node.
//It also creates blocks, which is a series of lox_ir_control_nodes that are executed sequentially without branches
//It also insert guards in:ç
//  - Return values
struct lox_ir_block * create_lox_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode,
        struct arena_lox_allocator * nodes_allocator,
        long options
);