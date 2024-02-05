#pragma once

#include "shared.h"
#include "compiler/scanner.h"

struct struct_definition {
    struct token name;
    struct token * field_names;
    int n_fields;
    bool is_public;
};

struct struct_instance {
    struct token name;
    struct struct_definition * struct_definition;
    struct struct_instance * next;
};

struct struct_definition * alloc_struct_definition();