#pragma once

#include "shared.h"

#include "types/types.h"
#include "types/string_object.h"
#include "types/array.h"
#include "utils/table.h"

struct struct_instance_object {
    struct object object;
    struct hash_table fields;
    struct struct_definition_object * definition;
};

struct struct_instance_object * alloc_struct_instance_object();

void init_struct_instance_object(struct struct_instance_object * struct_object);