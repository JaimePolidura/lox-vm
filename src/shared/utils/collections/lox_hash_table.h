#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared/utils/concurrency/rw_mutex.h"
#include "shared/types/string_object.h"
#include "shared/utils/utils.h"
#include "shared.h"

//Value, reference holder, extra ptr passed by the user in for_each_value_hash_table()
typedef void (*lox_hashtable_consumer_t)(
        struct string_object * key,
        struct string_object ** key_reference_holder,
        lox_value_t value,
        lox_value_t * value_reference_holder,
        void * extra
);

struct hash_table_entry {
    struct string_object * key;
    lox_value_t value;
};

struct lox_hash_table {
    struct lox_allocator * allocator;

    struct hash_table_entry * entries;
    struct rw_mutex rw_lock;
    int capacity;
    int size;
};

void for_each_value_hash_table(struct lox_hash_table * table, void * extra, lox_hashtable_consumer_t consumer);

//Get the value_node of the associated key. Returns NIL_VALUE if the element is not found
lox_value_t get_hash_table(struct lox_hash_table * table, struct string_object * key);

//Removes element from the table. Return true if the element was removed
bool remove_hash_table(struct lox_hash_table * table, struct string_object * key);

//Removes specific entry from the table.
void remove_entry_hash_table(struct hash_table_entry * entry);

//Puts the element in the map if it is present. Return true if the element was put
bool put_if_present_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value);

//Puts the element in the map if it is not present. Return true if the element was put
bool put_if_absent_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value);

//Puts the element in the map. Returns true if there was already an element with the same key
bool put_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value);

//Returns true if key is present in the map
bool contains_hash_table(struct lox_hash_table * table, struct string_object * key);

int get_req_capacity_lox_hash_table(int n_expected_elements);

//Returns the key value_node by its hash
struct string_object * get_key_by_hash(struct lox_hash_table * table, uint32_t keyHash);

void init_hash_table(struct lox_hash_table *, struct lox_allocator *);
void free_hash_table(struct lox_hash_table *);