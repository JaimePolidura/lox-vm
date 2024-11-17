#pragma once

#include "u64_hash_table.h"

struct u64_set {
    struct u64_hash_table inner_hash_table;
};

struct u64_set_iterator {
    struct u64_hash_table_iterator inner_hashtable_iterator;
};

void init_u64_set(struct u64_set *);
void free_u64_set(struct u64_set *);

void add_u64_set(struct u64_set *, uint64_t);
bool contains_u64_set(struct u64_set *, uint64_t);
uint8_t size_u64_set(struct u64_set);

void init_u64_set_iterator(struct u64_set_iterator *, struct u64_set);
bool has_next_u64_set_iterator(struct u64_set_iterator);
uint64_t next_u64_set_iterator(struct u64_set_iterator *);
