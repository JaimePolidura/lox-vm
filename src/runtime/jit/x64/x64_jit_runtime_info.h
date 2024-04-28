#pragma once

#include "shared.h"

//Used for storing data when changing from jit modes
struct x64_jit_runtime_info {
    //Stores rsp of jit mode
    uint64_t rsp;
    //Stores rbp of jit mode
    uint64_t rbp;
    //Stores self_thread pointer (stored in rbx) of jit mode
    uint64_t self_thread;
};