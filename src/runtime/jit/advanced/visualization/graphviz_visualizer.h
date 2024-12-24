#pragma once

#include "runtime/jit/advanced/optimizations/loop_invariant_code_motion.h"
#include "runtime/jit/advanced/optimizations/common_subexpression_elimination.h"
#include "runtime/jit/advanced/optimizations/sparse_constant_propagation.h"
#include "runtime/jit/advanced/optimizations/strength_reduction.h"
#include "runtime/jit/advanced/optimizations/copy_propagation.h"
#include "runtime/jit/advanced/optimizations/type_analysis.h"

#include "runtime/jit/advanced/creation/ssa_no_phis_creator.h"
#include "runtime/jit/advanced/creation/ssa_phi_inserter.h"
#include "runtime/jit/advanced/creation/ssa_phi_optimizer.h"
#include "runtime/jit/advanced/creation/ssa_creator.h"

#include "shared/utils/strings/string_builder.h"
#include "shared/types/function_object.h"
#include "shared/package.h"
#include "shared.h"

typedef enum {
    NO_PHIS_PHASE_SSA_GRAPHVIZ = 1 << 0,
    PHIS_INSERTED_PHASE_SSA_GRAPHVIZ = 1 << 1,
    PHIS_OPTIMIZED_PHASE_SSA_GRAPHVIZ = 1 << 2,

    SPARSE_CONSTANT_PROPAGATION_PHASE_SSA_GRAPHVIZ = 1 << 3,
    COMMON_SUBEXPRESSION_ELIMINATION_PHASE_SSA_GRAPHVIZ = 1 << 4,
    STRENGTH_REDUCTION_PHASE_SSA_GRAPHVIZ = 1 << 5,
    LOOP_INVARIANT_CODE_MOTION_PHASE_SSA_GRAPHVIZ = 1 << 6,
    COPY_PROPAGATION_PHASE_SSA_GRAPHVIZ = 1 << 7,
    TYPE_ANALYSIS_PHASE_SSA_GRAPHVIZ = 1 << 8,

    ALL_PHASE_SSA_GRAPHVIZ = 1 << 9,
} phase_ssa_graphviz_t;

enum {
    DEFAULT_GRAPHVIZ_OPT = 0,

    NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT = 1 << 0,
    NOT_DISPLAY_BLOCKS_GRAPHVIZ_OPT = 1 << 1,
    DISPLAY_TYPE_INFO_OPT = 1 << 2,
};

void generate_ssa_graphviz_graph(
        struct package * package,
        struct function_object * function,
        phase_ssa_graphviz_t phase,
        long graphviz_options,
        long ssa_options,
        char * path
);