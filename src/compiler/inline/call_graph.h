#pragma once

#include "compiler/compilation_result.h"
#include "compiler/function_call.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/package.h"
#include "shared.h"

struct call_graph;

struct call_graph_caller_data {
    int call_bytecode_index;
    bool is_inlined;
    struct call_graph * call_graph;
};

struct call_graph {
    struct package * package;
    struct function_object * function_object;
    scope_type_t scope;

    struct call_graph_caller_data ** children;
    int n_childs;
};

struct call_graph * create_call_graph(struct compilation_result *);