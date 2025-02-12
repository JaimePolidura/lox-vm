#pragma once

#include "runtime/jit/advanced/lox_ir.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/u64_set.h"
#include "shared/types/function_object.h"
#include "shared/package.h"

typedef enum {
    //LOX IR Creation
    NO_PHIS_PHASE_LOX_IR_VISUALIZATION = 1 << 0,
    PHIS_INSERTED_PHASE_LOX_IR_VISUALIZATION = 1 << 1,
    PHIS_OPTIMIZED_PHASE_LOX_IR_VISUALIZATION = 1 << 2,
    //Optimizations
    SPARSE_CONSTANT_PROPAGATION_PHASE_LOX_IR_VISUALIZATION = 1 << 3,
    COMMON_SUBEXPRESSION_ELIMINATION_PHASE_LOX_IR_VISUALIZATION = 1 << 4,
    STRENGTH_REDUCTION_PHASE_LOX_IR_VISUALIZATION = 1 << 5,
    LOOP_INVARIANT_CODE_MOTION_PHASE_LOX_IR_VISUALIZATION = 1 << 6,
    UNBOXING_INSERTION_PHASE_LOX_IR_VISUALIZATION = 1 << 7,
    TYPE_PROPAGATION_PHASE_LOX_IR_VISUALIZATION = 1 << 8,
    ESCAPE_ANALYSIS_PHASE_LOX_IR_VISUALIZATION = 1 << 9,
    COPY_PROPAGATION_PHASE_LOX_IR_VISUALIZATION = 1 << 10,

    //Machine code generation
    PHI_RESOLUTION_PHASE_LOX_IR_VISUALIZATION = 1 << 11,
    LOWERING_LOX_IR_VISUALIZATION = 1 << 12,

    ALL_PHASE_LOX_IR_VISUALIZATION = 1 << 63,
} phase_lox_ir_lox_ir_visualizer_t;

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
    struct lox_ir lox_ir;
    struct function_object * function;

    //These properties are only used in lox_ir_graphviz_visualizer:
    //Set of pointers of struct lox_ir_block visited
    struct u64_set blocks_graph_generated;
    //Last control control_node_to_lower graph id by block pointer
    struct u64_hash_table block_generated_graph_by_block;
    //Stores struct edge_graph_generated
    struct u64_set blocks_edges_generated;
    int next_block_id; //This is used in lox_ir_file_visualizer as well
    int next_control_node_id;
    int next_data_node_id;
};