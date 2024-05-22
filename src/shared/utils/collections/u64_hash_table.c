#include "u64_hash_table.h"

static struct u64_hash_table_entry * find_u64_hash_table_entry(struct u64_hash_table *, uint64_t key);
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
    struct u64_hash_table_entry * result = find_u64_hash_table_entry(hash_hable, key);
    return result != NULL ? result->value : NULL;
}

bool put_u64_hash_table(struct u64_hash_table * hash_hable, uint64_t key, void * value) {
    if (hash_hable->size + 1 >= hash_hable->capacity) {
        grow_u64_hash_table(hash_hable);
    }

    struct u64_hash_table_entry * entry = find_u64_hash_table_entry(hash_hable, key);
    bool key_already_exists = entry->value != NULL;
    entry->key = key;
    entry->value = value;

    if (!key_already_exists) {
        hash_hable->size++;
    }

    return key_already_exists;
}

static struct u64_hash_table_entry * find_u64_hash_table_entry(struct u64_hash_table * u64_hash_table, uint64_t key) {
    uint64_t index = key & (u64_hash_table->capacity - 1);
    struct u64_hash_table_entry * current_entry = u64_hash_table->entries + index;
    struct u64_hash_table_entry * start_entry = current_entry;

    while(current_entry != NULL && current_entry->value != NULL && current_entry->key != key){
        index = (index + 1) & (u64_hash_table->capacity - 1); //Optimized %
        current_entry = u64_hash_table->entries + index;
    }

    return current_entry;
}

static void grow_u64_hash_table(struct u64_hash_table * u64_hash_table) {
    uint64_t new_capacity = MAX(U64_HASH_TABLE_INITIAL_CAPACITY, u64_hash_table->capacity << 2);
    struct u64_hash_table_entry * new_entries = malloc(sizeof(struct u64_hash_table_entry) * new_capacity);
    memset(new_entries, 0, new_capacity * sizeof(struct u64_hash_table_entry));

    struct u64_hash_table_entry * old_entries = u64_hash_table->entries;
    uint64_t old_capacity = u64_hash_table->capacity;

    u64_hash_table->capacity = new_capacity;
    u64_hash_table->entries = new_entries;

    if (old_entries != NULL) {
        memcpy(new_entries, old_entries, old_capacity);
    }
}