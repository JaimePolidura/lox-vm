#pragma once

#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Annalizes the data flow and sets the produced type for each lox_ir_data_node. Also sets tp_allocator's lox_type_by_ssa_name_by_block
//The produced type will be "lox" types not native
//Safe to run at any point the compilation process
void perform_type_propagation(struct lox_ir *);
