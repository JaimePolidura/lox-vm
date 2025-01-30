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

    int last_v_reg_allocated;
};

//Removes the references in the struct lox_ir data structure to the ssa_name. It doest
//remove the nodes that uses the jit name
void remove_names_references_lox_ir(struct lox_ir *lox_ir, struct u64_set removed_ssa_names);
void remove_name_references_lox_ir(struct lox_ir *lox_ir, struct ssa_name ssa_name_to_remove);

struct ssa_name alloc_ssa_name_lox_ir(struct lox_ir *lox_ir, int ssa_version, char *local_name, struct lox_ir_block *block, struct lox_ir_type *type);
struct ssa_name alloc_ssa_version_lox_ir(struct lox_ir *lox_ir, int local_number);
void add_ssa_name_use_lox_ir(struct lox_ir *lox_ir, struct ssa_name ssa_name, struct lox_ir_control_node *control_node);
void remove_ssa_name_use_lox_ir(struct lox_ir *lox_ir, struct ssa_name ssa_name, struct lox_ir_control_node *control_node);

struct lox_ir_type * get_type_by_ssa_name_lox_ir(struct lox_ir *lox_ir, struct lox_ir_block *block, struct ssa_name ssa_name);
void put_type_by_ssa_name_lox_ir(struct lox_ir *lox_ir, struct lox_ir_block *block, struct ssa_name ssa_name, struct lox_ir_type *new_type);