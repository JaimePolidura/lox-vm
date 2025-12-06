#pragma once

#include "shared.h"

#include "shared/utils/collections/lox_arraylist.h"
#include "shared/utils/collections/lox_hash_table.h"
#include "shared/types/string_object.h"
#include "shared/types/types.h"

struct struct_instance_object {
    struct object object;
    struct lox_hash_table fields;
    struct struct_definition_object * definition;
};

void init_struct_instance_object(struct struct_instance_object *, struct struct_definition_object *);