#pragma once

#include "runtime/jit/advanced/optimizations/scp.h"
#include "runtime/jit/advanced/creation/ssa_creator.h"
#include "runtime/jit/advanced/creation/ssa_no_phis_creator.h"
#include "runtime/jit/advanced/creation/ssa_phi_inserter.h"
#include "runtime/jit/advanced/creation/ssa_phi_optimizer.h"

#include "shared/utils/strings/string_builder.h"
#include "shared/types/function_object.h"
#include "shared.h"
#include "shared/package.h"

typedef enum {
    NO_PHIS_PHASE_SSA_GRAPHVIZ,
    PHIS_INSERTED_PHASE_SSA_GRAPHVIZ,
    PHIS_OPTIMIZED_PHASE_SSA_GRAPHVIZ,
    SPARSE_CONSTANT_PROPAGATION_PHASE_SSA_GRAPHVIZ,
    ALL_PHASE_SSA_GRAPHVIZ,
} phase_ssa_graphviz_t;

void generate_ssa_graphviz_graph(
        struct package * package,
        struct function_object * function,
        phase_ssa_graphviz_t phase,
        char * path
);