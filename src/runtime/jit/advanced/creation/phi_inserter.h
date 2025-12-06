#pragma once

#include "runtime/jit/advanced/lox_ir_block.h"
#include "runtime/jit/advanced/lox_ir.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/u64_set.h"
#include "shared/utils/memory/arena.h"

//Inserts phi functions. Replaces nodes lox_ir_control_set_local_node with lox_ir_control_define_ssa_name_node and
//lox_ir_data_get_local_node with lox_ir_data_phi_node. These phase will add innecesary phi functions like: a1 = phi(a0)
void insert_lox_ir_phis(struct lox_ir *);