#pragma once

#include "compiler/compilation_result.h"
#include "compiler/bytecode/function_call.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/package.h"
#include "shared.h"

struct call_graph;

//This allows us to create a call graph of functions calls to other functions.
//This is useful when inlining functions, so we can iterate the graph avoiding cycles

struct call_graph_caller_data {
    int call_bytecode_index;
    bool is_inlined;
    struct call_graph * call_graph;
};

struct call_graph {
    struct package * package;
    struct function_object * function_object;

    struct call_graph_caller_data ** children;
    int n_childs;
};

struct call_graph_iterator {
    struct stack_list parents;
    struct stack_list pending;

    struct u64_hash_table already_checked;
};

struct call_graph * create_call_graph(struct compilation_result *);
void free_recursive_call_graph(struct call_graph *call_graph);

struct call_graph_iterator iterate_call_graph(struct call_graph *);
void free_call_graph_iterator(struct call_graph_iterator *);
bool has_next_call_graph(struct call_graph_iterator *);
struct call_graph * next_call_graph(struct call_graph_iterator *);