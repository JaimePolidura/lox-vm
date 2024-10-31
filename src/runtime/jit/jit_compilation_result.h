#pragma once

#include "shared/utils/collections/u8_arraylist.h"
#include "shared.h"
#include "shared/os/os_utils.h"

typedef void (*jit_compiled)(void);

struct jit_compilation_result {
    struct u8_arraylist compiled_code;
    bool success;
    //The JIT compilation failed, because other thread was compiling the same function
    bool failed_beacause_of_concurrent_compilation;
};

jit_compiled to_executable_jit_compiation_result(struct jit_compilation_result);