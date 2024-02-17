#include "table.h"

#define TABLE_MAX_LOAD 0.75

static inline bool is_tombstone(struct hash_table_entry * entry);

static struct hash_table_entry * find_entry(struct hash_table_entry * entries, int capacity, struct string_object * key);
static struct hash_table_entry * find_entry_by_hash(struct hash_table_entry * entries, int capacity, uint32_t key_hash);
static void adjust_hash_table_capacity(struct hash_table * table, int new_capacity);

void init_hash_table(struct hash_table * table) {
    table->size = 0;
    table->capacity = -1;
    table->entries = NULL;
    init_rw_mutex(&table->lock);
}

void add_all_hash_table(struct hash_table * to, struct hash_table * from) {
    for (int i = 0; i < from->capacity; i++) {
        struct hash_table_entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            put_hash_table(to, entry->key, entry->value);
        }
    }
}

struct string_object * get_key_by_hash(struct hash_table * table, uint32_t keyHash) {
    if (table->size == 0) {
        return NULL;
    }

    lock_reader_rw_mutex(&table->lock);

    struct hash_table_entry * entry = find_entry_by_hash(table->entries, table->capacity, keyHash);
    struct string_object * key_to_return = NULL;

    if(entry->key != NULL){
        key_to_return = entry->key;
    }

    unlock_reader_rw_mutex(&table->lock);

    return key_to_return;
}

bool contains_hash_table(struct hash_table * table, struct string_object * key){
    return get_hash_table(table, key, NULL);
}

bool get_hash_table(struct hash_table * table, struct string_object * key, lox_value_t *value){
    if (table->size == 0) {
        return false;
    }

    lock_reader_rw_mutex(&table->lock);

    struct hash_table_entry * entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        unlock_reader_rw_mutex(&table->lock);
        return false;
    }

    if(value != NULL){
        *value = entry->value;
    }

    unlock_reader_rw_mutex(&table->lock);

    return true;
}

bool put_if_absent_hash_table(struct hash_table * table, struct string_object * key, lox_value_t value) {
    if(table->size == 0){
        put_hash_table(table, key, value);
        return true;
    }

    lock_writer_rw_mutex(&table->lock);

    struct hash_table_entry * entry = find_entry(table->entries, table->capacity, key);
    bool element_added = false;

    if(entry->key == NULL){
        entry->value = value;
        entry->key = key;

        if (!is_tombstone(entry)) {
            table->size++;
        }

        element_added = true;
    }

    unlock_writer_rw_mutex(&table->lock);

    return element_added;
}

bool put_if_present_hash_table(struct hash_table * table, struct string_object * key, lox_value_t value) {
    if(table->size == 0){
        return false;
    }

    lock_writer_rw_mutex(&table->lock);

    struct hash_table_entry * entry = find_entry(table->entries, table->capacity, key);
    bool element_added = false;

    if(entry->key != NULL){
        entry->value = value;
        element_added = true;
    }

    unlock_writer_rw_mutex(&table->lock);

    return element_added;
}

bool remove_hash_table(struct hash_table * table, struct string_object * key) {
    if (table->size == 0) {
        return false;
    }

    lock_writer_rw_mutex(&table->lock);

    struct hash_table_entry * entry = find_entry(table->entries, table->capacity, key);
    bool element_removed = false;

    if (entry->key != NULL)  {
        remove_entry_hash_table(entry);
        element_removed = true;
    }

    unlock_writer_rw_mutex(&table->lock);

    return element_removed;
}

void remove_entry_hash_table(struct hash_table_entry * entry) {
    entry->key = NULL;
    entry->value = TO_LOX_VALUE_BOOL(true); //Tombstone, marked as deleted
}

bool put_hash_table(struct hash_table * table, struct string_object * key, lox_value_t value) {
    lock_writer_rw_mutex(&table->lock);

    if (table->size + 1 > table->capacity * TABLE_MAX_LOAD) {
        adjust_hash_table_capacity(table, GROW_CAPACITY(table->capacity));
    }

    struct hash_table_entry * entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;

    entry->key = key;
    entry->value = value;

    if (is_new_key && !is_tombstone(entry)) {
        table->size++;
    }

    unlock_writer_rw_mutex(&table->lock);

    return is_new_key;
}

static void adjust_hash_table_capacity(struct hash_table * table, int new_capacity) {
    struct hash_table_entry * new_entries = malloc(sizeof(struct hash_table_entry) * new_capacity);

    for (int i = 0; i < new_capacity; i++) {
        new_entries[i].key = NULL;
    }

    table->size = 0;

    for (int i = 0; i < table->capacity; i++) {
        struct hash_table_entry * entry = &table->entries[i];
        if (entry->key != NULL) {
            struct hash_table_entry * dest = find_entry(new_entries, new_capacity, entry->key);
            dest->key = entry->key;
            dest->value = entry->value;
            table->size++;
        }
    }

    free(table->entries);
    table->entries = new_entries;
    table->capacity = new_capacity;
}

void for_each_value_hash_table(struct hash_table * table, lox_type_consumer_t consumer) {
    for(int i = 0; i < table->capacity; i++){
        struct hash_table_entry * entry = &table->entries[i];

        if(entry->key != NULL && !is_tombstone(entry)){
            consumer(entry->value);
        }
    }
}

static struct hash_table_entry * find_entry(struct hash_table_entry * entries, int capacity, struct string_object * key) {
    return find_entry_by_hash(entries, capacity, key->hash);
}

static struct hash_table_entry * find_entry_by_hash(struct hash_table_entry * entries, int capacity, uint32_t key_hash) {
    struct hash_table_entry * first_tombstone_found = NULL;
    uint32_t index = key_hash & (capacity - 1); //Optimized %
    for (;;) {
        struct hash_table_entry * entry = &entries[index];

        if(is_tombstone(entry) && first_tombstone_found == NULL){
            first_tombstone_found = entry;
            continue;
        }
        if (entry->key != NULL && entry->key->hash == key_hash) {
            return entry;
        }
        if(entry->key == NULL && first_tombstone_found != NULL){
            return first_tombstone_found;
        }
        if(entry->key == NULL && first_tombstone_found == NULL){
            return entry;
        }

        index = (index + 1) & (capacity - 1); //Optimized %
    }
}

static inline bool is_tombstone(struct hash_table_entry * entry) {
    return entry->key == NULL && IS_BOOL(entry->value) && AS_BOOL(entry->value);
}