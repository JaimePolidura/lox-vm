#pragma once

#include "shared.h"
#include "table/table.h"

struct string_pool {
    struct hash_table strings;
};

struct string_pool_add_result {
    struct string_object * string_object;
    bool created_new;
};

void init_global_string_pool();

struct string_pool_add_result add_to_global_string_pool(char * string_ptr, int length);
