#pragma once

#include "shared.h"
#include "utils/table.h"
#include "utils/substring.h"
#include "utils/concurrency/mutex.h"

struct string_pool {
    struct hash_table strings;
    struct mutex lock;
};

struct string_pool_add_result {
    struct string_object * string_object;
    bool created_new;
};

void init_global_string_pool();

struct string_pool_add_result add_to_global_string_pool(char * string_ptr, int length);
struct string_pool_add_result add_substring_to_global_string_pool(struct substring substring);
void remove_entry_string_pool(struct string_object * key);