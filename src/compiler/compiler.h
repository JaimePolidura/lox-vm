#pragma once

#include "../shared.h"
#include "../vm/vm.h"
#include "scanner.h"
#include "../bytecode.h"
#include "../types/strings/string_object.h"

bool compile(char * source_code, struct chunk * output_chunk);
