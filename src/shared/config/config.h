#pragma once

#include "shared.h"

struct config {
    bool compile_inline_methods;

    struct {
        int eden_size_mb;
        int eden_block_size_kb;

        int survivor_size_mb;

        int old_size_mb;

        uint8_t n_generations_to_old;
    } generational_gc_config;
};