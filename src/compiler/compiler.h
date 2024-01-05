#pragma once

#include "../shared.h"
#include "../vm/vm.h"
#include "scanner.h"
#include "../bytecode.h"
#include "../types/strings/string_object.h"

struct compilation_result {
    struct chunk * chunk;
    bool success;
};

struct compilation_result compile(char * source_code);
