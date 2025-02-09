#pragma once

#include "runtime/jit/advanced/phi_resolution/phi_resolver.h"
#include "runtime/jit/advanced/optimizations/loop_invariant_code_motion.h"
#include "runtime/jit/advanced/optimizations/common_subexpression_elimination.h"
#include "runtime/jit/advanced/optimizations/sparse_constant_propagation.h"
#include "runtime/jit/advanced/optimizations/strength_reduction.h"
#include "runtime/jit/advanced/optimizations/unboxing_insertion.h"
#include "runtime/jit/advanced/optimizations/copy_propagation.h"
#include "runtime/jit/advanced/optimizations/type_propagation.h"
#include "runtime/jit/advanced/optimizations/escape_analysis.h"

#include "runtime/jit/advanced/creation/no_phis_creator.h"
#include "runtime/jit/advanced/creation/phi_inserter.h"
#include "runtime/jit/advanced/creation/phi_optimizer.h"
#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/strings/string_builder.h"
#include "shared/types/function_object.h"
#include "shared/package.h"
#include "shared.h"

typedef enum {
    //LOX IR Creation
    NO_PHIS_PHASE_LOX_IR_GRAPHVIZ = 1 << 0,
    PHIS_INSERTED_PHASE_LOX_IR_GRAPHVIZ = 1 << 1,
    PHIS_OPTIMIZED_PHASE_LOX_IR_GRAPHVIZ = 1 << 2,

    //Optimizations
    SPARSE_CONSTANT_PROPAGATION_PHASE_LOX_IR_GRAPHVIZ = 1 << 3,
    COMMON_SUBEXPRESSION_ELIMINATION_PHASE_LOX_IR_GRAPHVIZ = 1 << 4,
    STRENGTH_REDUCTION_PHASE_LOX_IR_GRAPHVIZ = 1 << 5,
    LOOP_INVARIANT_CODE_MOTION_PHASE_LOX_IR_GRAPHVIZ = 1 << 6,
    UNBOXING_INSERTION_PHASE_LOX_IR_GRAPHVIZ = 1 << 7,
    TYPE_PROPAGATION_PHASE_LOX_IR_GRAPHVIZ = 1 << 8,
    ESCAPE_ANALYSIS_PHASE_LOX_IR_GRAPHVIZ = 1 << 9,
    COPY_PROPAGATION_PHASE_LOX_IR_GRAPHVIZ = 1 << 10,

    //Phi control resolution / Virtual registers allocation
    PHI_RESOLUTION_PHASE_LOX_IR_GRAPHVIZ = 1 << 11,

    ALL_PHASE_LOX_IR_GRAPHVIZ = 1 << 63,
} phase_lox_ir_graphviz_t;

enum {
    DEFAULT_GRAPHVIZ_OPT = 0,

    NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT = 1 << 0,
    NOT_DISPLAY_BLOCKS_GRAPHVIZ_OPT = 1 << 1,
    DISPLAY_TYPE_INFO_OPT = 1 << 2,
    DISPLAY_ESCAPE_INFO_OPT = 1 << 3,
};

void generate_lox_ir_graphviz_graph(
        struct package * package,
        struct function_object * function,
        phase_lox_ir_graphviz_t phase,
        long graphviz_options,
        long options,
        char * path
);