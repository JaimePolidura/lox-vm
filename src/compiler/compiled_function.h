#pragma once

#include "shared.h"

struct compiled_function {
    struct function_object * function_object;
    struct struct_instance * struct_instances;
};