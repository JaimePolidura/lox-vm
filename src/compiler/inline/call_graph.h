#pragma once

#include "compiler/function_call.h"
#include "shared/package.h"
#include "shared.h"

struct call_graph {
    char * function_name;
    bool is_inlined;
    struct package * package;
    int call_bytecode_index;

    struct call_graph * children;
    int n_child;
};

struct call_graph * alloc_call_graph();
void init_call_graph(struct call_graph *);

void add_function_calls_call_graph(struct call_graph *, struct function_call *);