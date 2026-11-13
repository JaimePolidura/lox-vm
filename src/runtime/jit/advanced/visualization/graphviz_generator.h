#pragma once

#include "runtime/jit/advanced/creation/no_phis_creator.h"
#include "runtime/jit/advanced/creation/phi_inserter.h"
#include "runtime/jit/advanced/creation/phi_optimizer.h"
#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "runtime/jit/advanced/optimizations/common_subexpression_elimination.h"
#include "runtime/jit/advanced/optimizations/loop_invariant_code_motion.h"
#include "runtime/jit/advanced/optimizations/sparse_constant_propagation.h"
#include "runtime/jit/advanced/optimizations/strength_reduction.h"
#include "runtime/jit/advanced/optimizations/cast_insertion.h"
#include "runtime/jit/advanced/optimizations/copy_propagation.h"
#include "runtime/jit/advanced/optimizations/type_propagation.h"
#include "runtime/jit/advanced/optimizations/escape_analysis.h"
#include "runtime/jit/advanced/ir_lowerer/ir_lowerer.h"

#include "runtime/jit/advanced/register_allocation/register_allocator.h"

#include "shared/utils/strings/string_builder.h"
#include "shared/types/function_object.h"
#include "shared/package.h"
#include "shared.h"

enum {
    DEFAULT_GRAPHVIZ_OPT = 0,

    NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT = 1 << 0,
    NOT_DISPLAY_BLOCKS_GRAPHVIZ_OPT = 1 << 1,
    DISPLAY_TYPE_INFO_OPT = 1 << 2,
    DISPLAY_ESCAPE_INFO_OPT = 1 << 3,
};

struct lox_ir_visualizer {
    FILE * file;

    long graphviz_options;
    long options;
    struct lox_ir * lox_ir;
    struct function_object * function;

    //These properties are only used in lox_ir_graphviz_visualizer:
    //Set of pointers of struct lox_ir_block visited
    struct u64_set blocks_graph_generated;
    //Last control control_node_to_lower graph id by block pointer
    struct u64_hash_table block_generated_graph_by_block;
    //Stores struct edge_graph_generated
    struct u64_set blocks_edges_generated;
    int next_block_id;
    int next_control_node_id;
    int next_data_node_id;

    //These properties are only used in lox_ir_file_visualizer:
    int next_label_id;
    struct u64_hash_table labels_id_by_block;
    //Stored pointer to
    struct ptr_arraylist pend_cond_func_calls_to_add;

    // Only used when visualizing an interference grph
    struct u64_set ssa_names_generated;
};

//will generate a graph in graphviz format
void generate_graph_visualization_lox_ir(struct lox_ir_visualizer *, struct lox_ir_block * first_block);
void generate_graph_visualization_interference_graph(struct lox_ir_visualizer *, struct interference_graph *);