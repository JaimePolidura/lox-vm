#pragma once

#include "shared.h"

#include "types/types.h"
#include "types/string_object.h"
#include "types/array.h"

struct struct_object {
    struct object object;
    struct lox_array fields;
};

struct struct_object * alloc_struct_object();

void init_struct_object(struct struct_object * struct_object);
