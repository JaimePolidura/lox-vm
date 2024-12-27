#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

//Annalizes the data flow and sets the produced type for each ssa_data_node
//The produced type will be "lox" types not native
//Safe to run at any point the compilation process
void perform_type_propagation(struct ssa_ir *);
