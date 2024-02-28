#pragma once

#include "shared.h"

#include "types/types.h"
#include "types/string_object.h"
#include "utils/collections/lox/lox_array_list.h"
#include "utils/collections/lox/lox_hash_table.h"

struct struct_instance_object {
    struct object object;
    struct lox_hash_table fields;
    struct struct_definition_object * definition;
};

struct struct_instance_object * alloc_struct_instance_object();

void init_struct_instance_object(struct struct_instance_object * struct_object);