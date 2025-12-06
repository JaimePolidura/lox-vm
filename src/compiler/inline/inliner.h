#pragma once

#include "shared/utils/collections/stack_list.h"

#include "compiler/inline/function_inliner.h"
#include "compiler/compilation_result.h"

//This examines all the functions calls to find functions to inline.
struct compilation_result inline_bytecode_compilation(struct compilation_result bytecode_compilation);