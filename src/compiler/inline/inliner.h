#pragma once

#include "shared/utils/collections/stack_list.h"

#include "compiler/inline/call_graph_iterator.h"
#include "compiler/inline/function_inliner.h"
#include "compiler/compilation_result.h"

struct compilation_result inline_bytecode_compilation(struct compilation_result bytecode_compilation);