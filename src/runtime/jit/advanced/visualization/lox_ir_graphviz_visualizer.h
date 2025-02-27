#pragma once

#include "runtime/jit/advanced/visualization/lox_ir_visualizer_struct.h"

#include "runtime/jit/advanced/optimizations/common_subexpression_elimination.h"
#include "runtime/jit/advanced/optimizations/loop_invariant_code_motion.h"
#include "runtime/jit/advanced/optimizations/sparse_constant_propagation.h"
#include "runtime/jit/advanced/optimizations/strength_reduction.h"
#include "runtime/jit/advanced/optimizations/cast_insertion.h"
#include "runtime/jit/advanced/optimizations/copy_propagation.h"
#include "runtime/jit/advanced/optimizations/type_propagation.h"
#include "runtime/jit/advanced/optimizations/escape_analysis.h"
#include "runtime/jit/advanced/phi_resolution/phi_resolver.h"
#include "runtime/jit/advanced/ir_lowerer/ir_lowerer.h"

#include "runtime/jit/advanced/creation/no_phis_creator.h"
#include "runtime/jit/advanced/creation/phi_inserter.h"
#include "runtime/jit/advanced/creation/phi_optimizer.h"
#include "runtime/jit/advanced/creation/lox_ir_creator.h"

#include "shared/utils/strings/string_builder.h"
#include "shared/types/function_object.h"
#include "shared/package.h"
#include "shared.h"

//will generate a graph in graphviz format
void generate_graph_visualization_lox_ir(struct lox_ir_visualizer *, struct lox_ir_block * first_block);