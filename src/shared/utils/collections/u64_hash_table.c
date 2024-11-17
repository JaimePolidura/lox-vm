#include "u64_hash_table.h"

static struct u64_hash_table_entry * find_u64_hash_table_entry(struct u64_hash_table_entry * entries, int capacity, uint64_t key);
static void grow_u64_hash_table(struct u64_hash_table *);

void init_u64_hash_table(struct u64_hash_table * u64_hash_table) {
    u64_hash_table->size = 0;
    u64_hash_table->capacity = 0;
    u64_hash_table->entries = NULL;
}

struct u64_hash_table * alloc_u64_hash_table() {
    struct u64_hash_table * u64_hash_table = malloc(sizeof(struct u64_hash_table));
    init_u64_hash_table(u64_hash_table);
    return u64_hash_table;
}

void free_u64_hash_table(struct u64_hash_table * u64_hash_table) {
    free(u64_hash_table->entries);
}

void * get_u64_hash_table(struct u64_hash_table * hash_hable, uint64_t key) {
    if(hash_hable->size == 0){
        return NULL;
    }

    struct u64_hash_table_entry * result = find_u64_hash_table_entry(hash_hable->entries, hash_hable->capacity, key);
    return result != NULL ? result->value : NULL;
}

bool put_u64_hash_table(struct u64_hash_table * hash_hable, uint64_t key, void * value) {
    if (hash_hable->size + 1 >= hash_hable->capacity) {
        grow_u64_hash_table(hash_hable);
    }

    struct u64_hash_table_entry * entry = find_u64_hash_table_entry(hash_hable->entries, hash_hable->capacity, key);
    bool key_already_exists = entry->value != NULL;
    entry->key = key;
    entry->value = value;

    if (!key_already_exists) {
        hash_hable->size++;
    }

    return key_already_exists;
}

bool contains_u64_hash_table(struct u64_hash_table * table, uint64_t key) {
    return get_u64_hash_table(table, key) != NULL;
}

static struct u64_hash_table_entry * find_u64_hash_table_entry(struct u64_hash_table_entry * entries, int capacity, uint64_t key) {
    uint64_t index = key & (capacity - 1);
    struct u64_hash_table_entry * current_entry = entries + index;

    while (current_entry != NULL && current_entry->value != NULL && current_entry->key != key) {
        index = (index + 1) & (capacity - 1); //Optimized %
        current_entry = entries + index;
    }

    return current_entry;
}

static void grow_u64_hash_table(struct u64_hash_table * table) {
    uint64_t new_capacity = MAX(U64_HASH_TABLE_INITIAL_CAPACITY, table->capacity << 1);
    struct u64_hash_table_entry * new_entries = malloc(sizeof(struct u64_hash_table_entry) * new_capacity);
    struct u64_hash_table_entry * old_entries = table->entries;
    memset(new_entries, 0, new_capacity * sizeof(struct u64_hash_table_entry));

    for (int i = 0; i < table->capacity; i++) {
        struct u64_hash_table_entry * old_entry = &old_entries[i];
        if (old_entry->key != 0 && old_entry->value != NULL) {
            struct u64_hash_table_entry * new_entry = find_u64_hash_table_entry(new_entries, new_capacity, old_entry->key);
            new_entry->value = old_entry->value;
            new_entry->key = old_entry->key;
        }
    }

    table->entries = new_entries;
    table->capacity = new_capacity;
    free(old_entries);
}

void init_u64_hash_table_iterator(struct u64_hash_table_iterator * iterator, struct u64_hash_table hash_table) {
    iterator->hash_table = hash_table;
    iterator->current_index = 0;
    iterator->n_entries_returned = 0;
}

bool has_next_u64_hash_table_iterator(struct u64_hash_table_iterator iterator) {
    return iterator.n_entries_returned < iterator.hash_table.size;
}

struct u64_hash_table_entry next_u64_hash_table_iterator(struct u64_hash_table_iterator * iterator) {
    while(has_next_u64_hash_table_iterator(*iterator)){
        struct u64_hash_table_entry entry = iterator->hash_table.entries[iterator->current_index++];
        if(entry.key != 0 && entry.value != NULL){
            iterator->n_entries_returned++;
            return entry;
        }
    }

    //Unreachable code
    exit(-1);
}