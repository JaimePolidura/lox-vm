#pragma once

#include "shared.h"

#include "types.h"
#include "string_object.h"
#include "shared/utils/collections/lox_arraylist.h"
#include "shared/utils/collections/lox_hash_table.h"

struct struct_instance_object {
    struct object object;
    struct lox_hash_table fields;
    struct struct_definition_object * definition;
};

struct struct_instance_object * alloc_struct_instance_object(struct struct_definition_object *);

void init_struct_instance_object(struct struct_instance_object *, struct struct_definition_object *);