#pragma once

#include "compiler/compilation_result.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/inline/inliner.h"

//Main entrypoint for compilation

//Compile the code given a path and a package name
struct compilation_result compile(char * entrypoint_absolute_path, char * compilation_base_dir, char * package_name_entrypoint);

//Only compile the given source code
struct compilation_result compile_standalone(char * source_code);