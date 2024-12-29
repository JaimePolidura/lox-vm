#pragma once

#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/memory/arena.h"
#include "shared.h"

struct type_propagation_result {
    struct u64_hash_table ssa_type_by_ssa_name_by_block;
    struct arena_lox_allocator tp_allocator;
};

//Annalizes the data flow and sets the produced type for each ssa_data_node
//The produced type will be "lox" types not native
//Safe to run at any point the compilation process
struct type_propagation_result perform_type_propagation(struct ssa_ir *);
