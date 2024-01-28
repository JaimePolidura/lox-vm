#pragma once

struct compiler_struct {
    struct compiler_struct * next;
    struct token name;
    char ** field_names;
    int n_fields;
};