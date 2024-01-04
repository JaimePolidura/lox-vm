#pragma once

#include "../shared.h"
#include "../types/strings/string_object.h"

struct hash_table_entry {
    struct string_object * key;
    lox_value_t value;
};

struct hash_table {
    int size;
    int capacity;
    struct hash_table_entry * entries;
};

bool get_hash_table(struct hash_table * table, struct string_object * key, lox_value_t * value);
bool remove_hash_table(struct hash_table * table, struct string_object * key);
bool put_hash_table(struct hash_table * table, struct string_object * key, lox_value_t value);
void add_all_hash_table(struct hash_table * to, struct hash_table * from);
bool contains_hash_table(struct hash_table * table, struct string_object * key);
struct string_object * get_key_by_hash(struct hash_table * table, uint32_t keyHash);

void init_hash_table(struct hash_table * table);
