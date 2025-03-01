#include "lox_hash_table.h"

#define TABLE_MAX_LOAD 0.75

static inline bool is_tombstone(struct hash_table_entry * entry);

static struct hash_table_entry * find_entry(struct hash_table_entry * entries, int capacity, struct string_object * key);
static struct hash_table_entry * find_entry_by_hash(struct hash_table_entry * entries, int capacity, uint32_t key_hash);
static void adjust_hash_table_capacity(struct lox_hash_table * table, int new_capacity);

void init_hash_table(struct lox_hash_table * table, struct lox_allocator * allocator) {
    init_rw_mutex(&table->rw_lock);
    table->allocator = allocator;
    table->capacity = -1;
    table->entries = NULL;
    table->size = 0;
}

void free_hash_table(struct lox_hash_table * table) {
    LOX_FREE(table->allocator, table->entries);
    free_rw_mutex(&table->rw_lock);
}

void add_all_hash_table(struct lox_hash_table * to, struct lox_hash_table * from) {
    for (int i = 0; i < from->capacity; i++) {
        struct hash_table_entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            put_hash_table(to, entry->key, entry->value);
        }
    }
}

struct string_object * get_key_by_hash(struct lox_hash_table * table, uint32_t keyHash) {
    if (table->size == 0) {
        return NULL;
    }

    lock_reader_rw_mutex(&table->rw_lock);

    struct hash_table_entry * entry = find_entry_by_hash(table->entries, table->capacity, keyHash);
    struct string_object * key_to_return = NULL;

    if(entry->key != NULL){
        key_to_return = entry->key;
    }

    unlock_reader_rw_mutex(&table->rw_lock);

    return key_to_return;
}

bool contains_hash_table(struct lox_hash_table * table, struct string_object * key){
    return get_hash_table(table, key) != NULL;
}

lox_value_t get_hash_table(struct lox_hash_table * table, struct string_object * key){
    if (table->size == 0) {
        return NIL_VALUE;
    }

    lock_reader_rw_mutex(&table->rw_lock);
    struct hash_table_entry * entry = find_entry(table->entries, table->capacity, key);
    unlock_reader_rw_mutex(&table->rw_lock);

    if (entry->key == NULL) {
        return NIL_VALUE;
    }

    return entry->value;
}

bool put_if_absent_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value) {
    if(table->size == 0){
        put_hash_table(table, key, value);
        return true;
    }

    lock_writer_rw_mutex(&table->rw_lock);

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

    unlock_writer_rw_mutex(&table->rw_lock);

    return element_added;
}

bool put_if_present_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value) {
    if(table->size == 0){
        return false;
    }

    lock_writer_rw_mutex(&table->rw_lock);

    struct hash_table_entry * entry = find_entry(table->entries, table->capacity, key);
    bool element_added = false;

    if(entry->key != NULL){
        entry->value = value;
        element_added = true;
    }

    unlock_writer_rw_mutex(&table->rw_lock);

    return element_added;
}

bool remove_hash_table(struct lox_hash_table * table, struct string_object * key) {
    if (table->size == 0) {
        return false;
    }

    lock_writer_rw_mutex(&table->rw_lock);

    struct hash_table_entry * entry = find_entry(table->entries, table->capacity, key);
    bool element_removed = false;

    if (entry->key != NULL)  {
        remove_entry_hash_table(entry);
        element_removed = true;
    }

    unlock_writer_rw_mutex(&table->rw_lock);

    return element_removed;
}

void remove_entry_hash_table(struct hash_table_entry * entry) {
    entry->key = NULL;
    entry->value = TO_LOX_VALUE_BOOL(true); //Tombstone, marked value_as deleted
}

bool put_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value) {
    lock_writer_rw_mutex(&table->rw_lock);

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

    unlock_writer_rw_mutex(&table->rw_lock);

    return is_new_key;
}

static void adjust_hash_table_capacity(struct lox_hash_table * table, int new_capacity) {
    struct hash_table_entry * new_entries = LOX_MALLOC(table->allocator, sizeof(struct hash_table_entry) * new_capacity);

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

    LOX_FREE(table->allocator, table->entries);
    table->entries = new_entries;
    table->capacity = new_capacity;
}

void for_each_value_hash_table(struct lox_hash_table * table, void * extra, lox_hashtable_consumer_t consumer) {
    for (int i = 0; i < table->capacity; i++) {
        struct hash_table_entry * entry = &table->entries[i];

        if (entry->key != NULL && !is_tombstone(entry)) {
            consumer(entry->key, &entry->key, entry->value, &entry->value, extra);
        }
    }
}

int get_req_capacity_lox_hash_table(int n_expected_elements) {
    return round_up_8(n_expected_elements);
}

static struct hash_table_entry * find_entry(struct hash_table_entry * entries, int capacity, struct string_object * key) {
    return find_entry_by_hash(entries, capacity, key->hash);
}

static struct hash_table_entry * find_entry_by_hash(struct hash_table_entry * entries, int capacity, uint32_t key_hash) {
    struct hash_table_entry * first_tombstone_found = NULL;
    uint32_t index = key_hash & (capacity - 1); //Optimized %
    for (;;) {
        struct hash_table_entry * entry = &entries[index];
        index = (index + 1) & (capacity - 1);

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
    }
}

static inline bool is_tombstone(struct hash_table_entry * entry) {
    return entry->key == NULL && IS_BOOL(entry->value) && AS_BOOL(entry->value);
}