#pragma once

#include "runtime/jit/advanced/creation/ssa_phi_optimizer.h"
#include "runtime/jit/advanced/creation/ssa_phi_inserter.h"
#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/ssa_block.h"

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/queue_list.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/utils/memory/arena.h"
#include "shared/package.h"

#define SSA_IR_NODE_LOX_ALLOCATOR(ssa_ir) (&(ssa_ir)->ssa_nodes_allocator_arena->lox_allocator)

struct ssa_ir {
    struct ssa_block * first_block;
    //SSA Definitions nodes by ssa name
    struct u64_hash_table ssa_definitions_by_ssa_name;
    //Max version allocated per local number
    struct u8_hash_table max_version_allocated_per_local;
    //key: ssa_name, value: u64_set of pointers ssa_control_nodes
    struct u64_hash_table node_uses_by_ssa_name;
    //All control, data & blocks will be allocated in the arena
    struct arena_lox_allocator * ssa_nodes_allocator_arena;
    //Function of the ssa_ir
    struct function_object * function;
};

//Removes the references in the struct ssa_ir data structure to the ssa_name. It doest
//remove the nodes that uses the ssa name
void remove_names_references_ssa_ir(struct ssa_ir *, struct u64_set);
void remove_name_references_ssa_ir(struct ssa_ir *, struct ssa_name);

struct ssa_name alloc_ssa_name_ssa_ir(struct ssa_ir *, int ssa_version, char *local_name);
void add_ssa_name_use_ssa_ir(struct ssa_ir *, struct ssa_name, struct ssa_control_node *);