#pragma once

#include "../../shared.h"
#include "../../table/table.h"

struct string_pool {
    struct hash_table strings;
};

struct string_pool_add_result {
    struct string_object * string_object;
    bool created_new;
};

void init_string_pool(struct string_pool * pool);

struct string_pool_add_result add_copy_of_string_pool(struct string_pool * pool, char * string_ptr, int length);
struct string_pool_add_result add_string_pool(struct string_pool * pool, char * string_ptr, int length);
void add_all_pool(struct string_pool * to, struct string_pool * from);