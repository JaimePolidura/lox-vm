#pragma once

#include "shared.h"
#include "compiler/scanner.h"
#include "utils/table.h"

struct struct_definition {
    struct token name;
    struct string_object * field_names;
    int n_fields;
    bool is_public;
};

struct struct_definition * alloc_struct_definition();