#pragma once

#include "shared/utils/collections/u8_arraylist.h"
#include "shared.h"

struct jit_compilation_result {
    struct u8_arraylist compiled_code;
    bool success;
};