#pragma once

#include "shared.h"

//Used for storing data when changing from jit modes
struct x64_jit_runtime_info {
    uint64_t rsp;
    uint64_t rbp;
    uint64_t self_thread;
};