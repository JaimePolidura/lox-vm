#pragma once

#include "compiler/bytecode/chunk/chunk_iterator.h"
#include "compiler/bytecode/chunk/chunk.h"
#include "shared/bytecode/bytecode.h"

#include "shared/types/string_object.h"
#include "shared/types/function_object.h"
#include "shared/types/struct_definition_object.h"
#include "shared/types/package_object.h"
#include "shared.h"

#define DISASSEMBLE_ONLY_MAIN 0
#define DISASSEMBLE_PACKAGE_FUNCTIONS 1

void disassemble_package(struct package * package, long options);