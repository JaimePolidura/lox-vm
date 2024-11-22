#pragma once

#include "shared/utils/utils.h"
#include "shared.h"

#define U64_HASH_TABLE_INITIAL_CAPACITY 8

//Regular hash table which uses the uint64 value_as an index
struct u64_hash_table_entry {
    uint64_t key;
    void * value;
    bool some_value;
};

struct u64_hash_table {
    int capacity;
    int size;
    struct u64_hash_table_entry * entries;
};

struct u64_hash_table_iterator {
    struct u64_hash_table hash_table;

    int n_entries_returned;
    int current_index;
};


void init_u64_hash_table(struct u64_hash_table *);
struct u64_hash_table * alloc_u64_hash_table();
void free_u64_hash_table(struct u64_hash_table *);

//Returns the element of the hash table given a key. Returns NULL if it wasn't found
void * get_u64_hash_table(struct u64_hash_table *, uint64_t key);

//Puts the element in the hash table. Returns true if there was already an element with the same key
bool put_u64_hash_table(struct u64_hash_table *, uint64_t key, void * value);

//Returns true if element with key is present on the map
bool contains_u64_hash_table(struct u64_hash_table *, uint64_t key);

void init_u64_hash_table_iterator(struct u64_hash_table_iterator *, struct u64_hash_table);
bool has_next_u64_hash_table_iterator(struct u64_hash_table_iterator iterator);
struct u64_hash_table_entry next_u64_hash_table_iterator(struct u64_hash_table_iterator * iterator);