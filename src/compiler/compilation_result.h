#pragma once

#include "shared/utils/collections/ptr_arraylist.h"

struct compilation_result {
    bool success;

    struct package * compiled_package;
    uint32_t n_compiled_packages;
    uint32_t n_locals;

    char * error_message;
};