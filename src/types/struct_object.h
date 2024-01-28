#pragma once

#include "shared.h"

#include "types/types.h"
#include "types/string_object.h"

struct struct_object {
    struct object object;
    lox_value_t * fields;
    int n_fields;
};

struct struct_object * alloc_struct_object();

void init_struct_object(struct struct_object * struct_object);
