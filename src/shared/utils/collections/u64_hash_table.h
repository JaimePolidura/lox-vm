#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared/utils/utils.h"
#include "shared.h"

#define U64_HASH_TABLE_INITIAL_CAPACITY 8

#define EMPTY_U64_HASH_TABLE_ENTRY() ((struct u64_hash_table_entry) {.key = 0, .value = NULL, .state = U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT} )

#define FOR_EACH_U64_HASH_TABLE_ENTRY(hash_table, entry) \
    struct u64_hash_table_iterator iterator##entry; \
    init_u64_hash_table_iterator(&iterator##entry, (hash_table)); \
    struct u64_hash_table_entry entry = has_next_u64_hash_table_iterator(iterator##entry) ? \
        next_u64_hash_table_iterator(&iterator##entry) : EMPTY_U64_HASH_TABLE_ENTRY(); \
    for(int i##value = 0; \
        i##value < (hash_table).size; \
        i##value++, entry = has_next_u64_hash_table_iterator(iterator##entry) ? next_u64_hash_table_iterator(&iterator##entry) : EMPTY_U64_HASH_TABLE_ENTRY())


//Regular hash table which uses the uint64 value_as an index
struct u64_hash_table_entry {
    enum {
        U64_HASH_TABLE_ENTRY_STATE_EMTPY,
        U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT,
        U64_HASH_TABLE_ENTRY_STATE_TOMBSTONE,
    } state;
    uint64_t key;
    void * value;
};

struct u64_hash_table {
    struct lox_allocator * allocator;

    int capacity;
    int size;
    struct u64_hash_table_entry * entries;
};

struct u64_hash_table_iterator {
    struct u64_hash_table hash_table;

    int n_entries_returned;
    int current_index;
};


void init_u64_hash_table(struct u64_hash_table *, struct lox_allocator *);
struct u64_hash_table * alloc_u64_hash_table(struct lox_allocator *);
void free_u64_hash_table(struct u64_hash_table *);

void clear_u64_hash_table(struct u64_hash_table *);

//Returns the element of the hash table given a key. Returns NULL if it wasn't found
void * get_u64_hash_table(struct u64_hash_table *, uint64_t key);

//Puts the element in the hash table. Returns true if there was already an element with the same key
bool put_u64_hash_table(struct u64_hash_table *, uint64_t key, void * value);

//Returns true if element with key is present on the map
bool contains_u64_hash_table(struct u64_hash_table *, uint64_t key);

void remove_u64_hash_table(struct u64_hash_table *, uint64_t key);

struct u64_hash_table * clone_u64_hash_table(struct u64_hash_table *, struct lox_allocator *);

void init_u64_hash_table_iterator(struct u64_hash_table_iterator *, struct u64_hash_table);
bool has_next_u64_hash_table_iterator(struct u64_hash_table_iterator iterator);
struct u64_hash_table_entry next_u64_hash_table_iterator(struct u64_hash_table_iterator * iterator);