#pragma once

#include "../shared.h"
#include "../vm/vm.h"
#include "scanner.h"
#include "../bytecode.h"
#include "../types/strings/string_object.h"
#include "types/function.h"

struct compilation_result {
    struct function_object * function_object;
    struct chunk * chunk;
    bool success;
};

struct compilation_result compile(char * source_code);
