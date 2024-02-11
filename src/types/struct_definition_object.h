#pragma once

#include "shared.h"

#include "types/types.h"
#include "types/string_object.h"
#include "types/array.h"
#include "utils/table.h"
#include "compiler/compiler_structs.h"

struct struct_definition_object {
    struct object object;
    struct struct_definition * definition;
};

struct struct_definition_object * alloc_struct_definition_object();

void init_struct_definition_object(struct struct_definition_object * struct_object);
