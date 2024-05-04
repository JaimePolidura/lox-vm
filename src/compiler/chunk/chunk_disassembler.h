#pragma once

#include "shared.h"
#include "chunk.h"
#include "compiler/bytecode.h"
#include "shared/types/string_object.h"
#include "shared/types/function_object.h"
#include "shared/types/struct_definition_object.h"
#include "shared/types/package_object.h"

#define DISASSEMBLE_PACKAGE_FUNCTIONS 1

void disassemble_package(struct package * package, long options);