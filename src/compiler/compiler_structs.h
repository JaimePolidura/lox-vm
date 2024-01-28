#pragma once

#include "shared.h"
#include "compiler/scanner.h"

struct compiler_struct {
    struct compiler_struct * next;
    struct token name;
    struct token * field_names;
    int n_fields;
};

struct compiler_struct * alloc_compiler_struct();