#pragma once

#include "shared.h"
#include "shared/types/string_object.h"
#include "shared/utils/concurrency/rw_mutex.h"
#include "shared/utils/utils.h"

struct hash_table_entry {
    struct string_object * key;
    lox_value_t value;
};

struct lox_hash_table {
    int size;
    int capacity;
    struct rw_mutex rw_lock;
    struct hash_table_entry * entries;
};

void for_each_value_hash_table(struct lox_hash_table * table, lox_type_consumer_t consumer);

//Get the value of the associated key in the value parameter. Returns false if the element is not found
bool get_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t * value);

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

//Returns the key value by its hash
struct string_object * get_key_by_hash(struct lox_hash_table * table, uint32_t keyHash);

void init_hash_table(struct lox_hash_table * table);
void free_hash_table(struct lox_hash_table * table);