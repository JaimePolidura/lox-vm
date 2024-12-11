#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "u64_hash_table.h"

struct u64_set {
    struct u64_hash_table inner_hash_table;
};

struct u64_set_iterator {
    struct u64_hash_table_iterator inner_hashtable_iterator;
};

#define FOR_EACH_U64_SET_VALUE(set, value) \
    struct u64_set_iterator iterator##value; \
    init_u64_set_iterator(&iterator##value, (set)); \
    uint64_t value = has_next_u64_set_iterator(iterator##value) ? next_u64_set_iterator(&iterator##value) : 0; \
    for(int i##value = 0; \
        i##value < (set).inner_hash_table.size; \
        i##value++, value = has_next_u64_set_iterator(iterator##value) ? next_u64_set_iterator(&iterator##value) : 0)

struct u64_set empty_u64_set(struct lox_allocator *);
struct u64_set * alloc_u64_set(struct lox_allocator *);
void init_u64_set(struct u64_set *, struct lox_allocator *);
void free_u64_set(struct u64_set *);

void add_u64_set(struct u64_set *, uint64_t);
bool contains_u64_set(struct u64_set *, uint64_t);
uint8_t size_u64_set(struct u64_set);
uint64_t get_first_value_u64_set(struct u64_set);
void union_u64_set(struct u64_set *, struct u64_set);
void remove_u64_set(struct u64_set *, uint64_t);
void clear_u64_set(struct u64_set *);
//Returns true if b is subset of a
bool is_subset_u64_set(struct u64_set a, struct u64_set b);
//Example: a: {1, 2} b: {2, 3}, a - b = {1}
void difference_u64_set(struct u64_set * a, struct u64_set b);
void intersection_u64_set(struct u64_set * a, struct u64_set b);
struct u64_set clone_u64_set(struct u64_set *, struct lox_allocator *);

void init_u64_set_iterator(struct u64_set_iterator *, struct u64_set);
bool has_next_u64_set_iterator(struct u64_set_iterator);
uint64_t next_u64_set_iterator(struct u64_set_iterator *);
