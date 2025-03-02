#pragma once

#include "runtime/jit/advanced/creation/phi_optimizer.h"
#include "runtime/jit/advanced/creation/phi_inserter.h"
#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/lox_ir_block.h"

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/queue_list.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/utils/memory/arena.h"
#include "shared/package.h"

#define LOX_IR_ALLOCATOR(lox_ir) (&(lox_ir)->nodes_allocator_arena->lox_allocator)

struct lox_ir {
    struct lox_ir_block * first_block;
    //SSA Definitions nodes by jit name
    struct u64_hash_table definitions_by_ssa_name;
    //Max version allocated per local number
    struct u8_hash_table max_version_allocated_per_local;
    //key: ssa_name, value: u64_set of pointers control_nodes
    struct u64_hash_table node_uses_by_ssa_name;
    //All control, data & blocks will be allocated in the arena
    struct arena_lox_allocator * nodes_allocator_arena;
    //Function of the lox_ir
    struct function_object * function;
    //Set by type_propagation Key: block pointer, value: Hashtable of key ssa_name, value: lox_ir_type *.
    struct u64_hash_table type_by_ssa_name_by_block;
    //Key: ssa_name, value: boolean that indicates is the ssa name definitino is cyclical
    struct u64_hash_table cyclic_ssa_name_definitions;

    int last_v_reg_allocated;
    //key: v register number, value: u64_set of pointers to control_nodes
    struct u64_hash_table definitions_by_v_reg;
    //key: v register number, value: u64_set of pointers control_nodes
    struct u64_hash_table node_uses_by_v_reg;
};

//Removes control node from block. If the control node to remove is the only one in the block, the block will get removed
//from the lox ir graph
void remove_block_control_node_lox_ir(struct lox_ir*, struct lox_ir_block*, struct lox_ir_control_node*);
//Removes block from lox ir graph. Expect:
// - the block to be removed is not the last one in the lox ir
// - if the set size of successors size is more than 1, the set size of predecessors is 1 and vice versa, both sets size
//  cannot exceed 1 at the same time
// - if the block to remove is the first block of lox ir, the successors set size shouldn't exceeed 1
void remove_only_block_lox_ir(struct lox_ir*, struct lox_ir_block*);
//Removes a true/false branch/block and the subsequent children of the branch/block to remove (subgraph)
struct branch_removed {
    struct u64_set ssa_name_definitions_removed;
    struct u64_set blocks_removed;
};
struct branch_removed remove_block_branch_lox_ir(struct lox_ir*, struct lox_ir_block*, bool, struct lox_allocator*);
//a dominates b
bool dominates_block_lox_ir(struct lox_ir*, struct lox_ir_block*, struct lox_ir_block*, struct lox_allocator*);
struct u64_set get_block_dominator_set_lox_ir(struct lox_ir*, struct lox_ir_block*, struct lox_allocator*);
//Returns all the possible blocks that might be traversed from a to b. If there is no such possible paths, it will return
//all the nodes from a to the end of the graph
struct u64_set get_all_block_paths_to_block_set_lox_ir(struct lox_ir*,struct lox_ir_block*a,struct lox_ir_block*b,struct lox_allocator*);
void insert_block_before_lox_ir(struct lox_ir*, struct lox_ir_block*, struct u64_set predecessors, struct lox_ir_block* successor);

//Removes the references in the struct lox_ir data structure to the ssa_name. It doest
//remove the nodes that uses the jit name
void remove_names_references_lox_ir(struct lox_ir *lox_ir, struct u64_set removed_ssa_names);
void remove_name_references_lox_ir(struct lox_ir *lox_ir, struct ssa_name ssa_name_to_remove);

struct ssa_name alloc_ssa_name_lox_ir(struct lox_ir *lox_ir, int ssa_version, char *local_name, struct lox_ir_block *block, struct lox_ir_type *type);
struct ssa_name alloc_ssa_version_lox_ir(struct lox_ir *lox_ir, int local_number);
void add_ssa_name_use_lox_ir(struct lox_ir *lox_ir, struct ssa_name ssa_name, struct lox_ir_control_node *control_node);
void remove_ssa_name_use_lox_ir(struct lox_ir *lox_ir, struct ssa_name ssa_name, struct lox_ir_control_node *control_node);
bool is_cyclic_ssa_name_definition_lox_ir(struct lox_ir *, struct ssa_name, struct ssa_name);
void on_ssa_name_def_moved_lox_ir(struct lox_ir *, struct ssa_name);

struct lox_ir_type * get_type_by_ssa_name_lox_ir(struct lox_ir *lox_ir, struct lox_ir_block *block, struct ssa_name ssa_name);
void put_type_by_ssa_name_lox_ir(struct lox_ir *lox_ir, struct lox_ir_block *block, struct ssa_name ssa_name, struct lox_ir_type *new_type);

struct v_register alloc_v_register_lox_ir(struct lox_ir*, bool);
void add_v_register_use_lox_ir(struct lox_ir *lox_ir, int, struct lox_ir_control_node*);
void add_v_register_definition_lox_ir(struct lox_ir *lox_ir, int, struct lox_ir_control_node*);
