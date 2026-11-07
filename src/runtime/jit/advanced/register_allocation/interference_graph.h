#pragma once

#include "runtime/jit/advanced/lox_ir.h"
#include "runtime/jit/advanced/register_allocation/live_range_analysis.h"

struct interference_graph {
    struct u64_hash_table nodes;
};

struct interference_graph_node {
    struct ssa_name ssa_name;
    //u64 ssa name -> to struct interference_graph_node *
    struct u64_hash_table links;
};

struct interference_graph build_interference_graph(struct lox_ir*, struct ptr_arraylist*);