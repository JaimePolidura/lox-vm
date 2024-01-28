#pragma once

#include "shared.h"
#include "compiler/scanner.h"

struct struct_definition {
    struct struct_definition * next;
    struct token name;
    struct token * field_names;
    int n_fields;
};

struct struct_definition * alloc_compiler_struct();