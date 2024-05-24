#pragma once

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"

#include "compiler/inline/call_graph.h"

//Post order no recursive implementation
struct call_graph_iterator {
    struct stack_list parents;
    struct stack_list pending;

    struct u64_hash_table already_checked;
};

struct call_graph_iterator iterate_call_graph(struct call_graph *);

void free_call_graph_iterator(struct call_graph_iterator *);

bool has_next_call_graph(struct call_graph_iterator *);

struct call_graph * next_call_graph(struct call_graph_iterator *);
