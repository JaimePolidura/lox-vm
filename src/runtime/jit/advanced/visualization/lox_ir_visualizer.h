#pragma once

#include "runtime/jit/advanced/visualization/graphviz_generator.h"

#include "runtime/jit/advanced/creation/no_phis_creator.h"
#include "runtime/jit/advanced/creation/phi_inserter.h"
#include "runtime/jit/advanced/creation/phi_optimizer.h"
#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "runtime/jit/advanced/optimizations/loop_invariant_code_motion.h"
#include "runtime/jit/advanced/optimizations/common_subexpression_elimination.h"
#include "runtime/jit/advanced/optimizations/sparse_constant_propagation.h"
#include "runtime/jit/advanced/optimizations/strength_reduction.h"
#include "runtime/jit/advanced/optimizations/cast_insertion.h"
#include "runtime/jit/advanced/optimizations/copy_propagation.h"
#include "runtime/jit/advanced/optimizations/type_propagation.h"
#include "runtime/jit/advanced/optimizations/escape_analysis.h"

#include "runtime/jit/advanced/register_allocation/register_allocator.h"

#include "shared/utils/strings/string_builder.h"
#include "shared/types/function_object.h"
#include "shared/package.h"
#include "shared.h"

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
        TYPE_PROPAGATION_PHASE_LOX_IR_VISUALIZATION = 1 << 7,
        CAST_INSERTION_PHASE_LOX_IR_VISUALIZATION = 1 << 8,
        ESCAPE_ANALYSIS_PHASE_LOX_IR_VISUALIZATION = 1 << 9,
        COPY_PROPAGATION_PHASE_LOX_IR_VISUALIZATION = 1 << 10,
        //Machine code generation
        LOWERING_LOX_IR_VISUALIZATION = 1 << 11,
        INTERFERENCE_GRAPH_LOX_IR_VISUALIZATION = 1 << 12,

        ALL_PHASE_LOX_IR_VISUALIZATION = 1 << 63,
} phase_lox_ir_lox_ir_visualizer_t;

//Entry point to visualize the lox ir graph
//This might leak heap memory, So use it only once
void visualize(
        struct package * package,
        struct function_object * function,
        phase_lox_ir_lox_ir_visualizer_t phase,
        long graphviz_options,
        long options,
        char * path
);