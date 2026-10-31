#pragma once

#include "runtime/jit/advanced/lox_ir.h"
#include "runtime/jit/advanced/register_allocation/live_range_analysis.h"

struct interference_graph_node {
    //ssa name points to struct interference_graph_node *
    struct u64_hash_table links;
};

void build_interference_graph(struct lox_ir*, struct ptr_arraylist*);