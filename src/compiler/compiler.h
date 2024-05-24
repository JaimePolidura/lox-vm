#pragma once

#include "compiler/compilation_result.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/inline/inliner.h"

//Main entrypoint for compilation

struct compilation_result compile(char * entrypoint_absolute_path, char * compilation_base_dir, char * package_name_entrypoint);

struct compilation_result compile_standalone(char * source_code);