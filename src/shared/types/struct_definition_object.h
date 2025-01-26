#pragma once

#include "shared.h"

#include "types.h"
#include "string_object.h"
#include "shared/utils/collections/lox_arraylist.h"
#include "shared/utils/collections/lox_hash_table.h"

struct struct_definition_object {
    struct object object;
    struct string_object * name;
    struct string_object ** field_names;
    int n_fields;
    bool is_public;
};

struct struct_definition_object * alloc_struct_definition_object();

void init_struct_definition_object(struct struct_definition_object*);

