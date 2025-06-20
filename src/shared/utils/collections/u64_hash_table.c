#include "u64_hash_table.h"

#define TABLE_MAX_LOAD 0.75

static struct u64_hash_table_entry * find_u64_hash_table_entry(struct u64_hash_table_entry * entries, int capacity, uint64_t key);
static void grow_u64_hash_table(struct u64_hash_table *);
extern void runtime_panic(char * format, ...);

void init_u64_hash_table(struct u64_hash_table * u64_hash_table, struct lox_allocator * allocator) {
    u64_hash_table->allocator = allocator;
    u64_hash_table->entries = NULL;
    u64_hash_table->capacity = 0;
    u64_hash_table->size = 0;
}

struct u64_hash_table * alloc_u64_hash_table(struct lox_allocator * allocator) {
    struct u64_hash_table * u64_hash_table = LOX_MALLOC(allocator, sizeof(struct u64_hash_table));
    init_u64_hash_table(u64_hash_table, allocator);
    return u64_hash_table;
}

void free_u64_hash_table(struct u64_hash_table * u64_hash_table) {
    LOX_FREE(u64_hash_table->allocator, u64_hash_table->entries);
}

void * get_u64_hash_table(struct u64_hash_table * hash_hable, uint64_t key) {
    if(hash_hable->size == 0){
        return NULL;
    }

    struct u64_hash_table_entry * result = find_u64_hash_table_entry(hash_hable->entries, hash_hable->capacity, key);
    return result != NULL && result->state == U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT ? result->value : NULL;
}

bool put_u64_hash_table(struct u64_hash_table * hash_hable, uint64_t key, void * value) {
    if (hash_hable->size + 1 >= hash_hable->capacity * TABLE_MAX_LOAD) {
        grow_u64_hash_table(hash_hable);
    }

    struct u64_hash_table_entry * entry = find_u64_hash_table_entry(hash_hable->entries, hash_hable->capacity, key);
    bool key_already_exists = entry->state == U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT;
    entry->key = key;
    entry->value = value;
    entry->state = U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT;

    if (!key_already_exists) {
        hash_hable->size++;
    }

    return key_already_exists;
}

bool contains_u64_hash_table(struct u64_hash_table * hash_hable, uint64_t key) {
    if(hash_hable->size == 0){
        return false;
    }

    struct u64_hash_table_entry * entry = find_u64_hash_table_entry(hash_hable->entries, hash_hable->capacity, key);
    return entry != NULL && entry->state == U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT;
}

void remove_u64_hash_table(struct u64_hash_table * hash_hable, uint64_t key) {
    struct u64_hash_table_entry * entry = find_u64_hash_table_entry(hash_hable->entries, hash_hable->capacity, key);
    if(entry != NULL && entry->state == U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT) {
        entry->state = U64_HASH_TABLE_ENTRY_STATE_TOMBSTONE;
        hash_hable->size--;
    }
}

void clear_u64_hash_table(struct u64_hash_table * hash_table) {
    hash_table->size = 0;
    for(int i = 0; i < hash_table->capacity; i++){
        struct u64_hash_table_entry * current_entry = hash_table->entries + i;
        current_entry->state = U64_HASH_TABLE_ENTRY_STATE_EMTPY;
    }
}

static struct u64_hash_table_entry * find_u64_hash_table_entry(struct u64_hash_table_entry * entries, int capacity, uint64_t key) {
    if (capacity == 0) {
        return NULL;
    }

    uint64_t index = key & (capacity - 1);
    struct u64_hash_table_entry * last_tombstone_found_entry = NULL;

    for (;;) {
        struct u64_hash_table_entry * current_entry = entries + index;
        index = (index + 1) & (capacity - 1); //Optimized %

        if (current_entry->state == U64_HASH_TABLE_ENTRY_STATE_TOMBSTONE && last_tombstone_found_entry == NULL) {
            last_tombstone_found_entry = current_entry;
            continue;
        }
        if (current_entry->state == U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT && current_entry->key == key) {
            return current_entry;
        }
        if (current_entry->state == U64_HASH_TABLE_ENTRY_STATE_EMTPY && last_tombstone_found_entry != NULL) {
            return last_tombstone_found_entry;
        }
        if (current_entry->state == U64_HASH_TABLE_ENTRY_STATE_EMTPY && last_tombstone_found_entry == NULL) {
            return current_entry;
        }
    }
}

static void grow_u64_hash_table(struct u64_hash_table * table) {
    uint64_t new_capacity = MAX(U64_HASH_TABLE_INITIAL_CAPACITY, table->capacity << 1);

    struct u64_hash_table_entry * new_entries = LOX_MALLOC(table->allocator, sizeof(struct u64_hash_table_entry) * new_capacity);
    struct u64_hash_table_entry * old_entries = table->entries;
    memset(new_entries, 0, new_capacity * sizeof(struct u64_hash_table_entry));

    for (int i = 0; i < table->capacity; i++) {
        struct u64_hash_table_entry * old_entry = &old_entries[i];
        if (old_entry->state == U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT) {
            struct u64_hash_table_entry * new_entry = find_u64_hash_table_entry(new_entries, new_capacity, old_entry->key);
            new_entry->state = U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT;
            new_entry->value = old_entry->value;
            new_entry->key = old_entry->key;
        }
    }

    table->entries = new_entries;
    table->capacity = new_capacity;
    LOX_FREE(table->allocator, old_entries);
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
    while (has_next_u64_hash_table_iterator(*iterator)) {
        struct u64_hash_table_entry entry = iterator->hash_table.entries[iterator->current_index++];
        if (entry.state == U64_HASH_TABLE_ENTRY_STATE_VALUE_PRESENT) {
            iterator->n_entries_returned++;
            return entry;
        }
    }

    runtime_panic("Illegal state of u64_hash_table_iterator. Expect call to has_next() before call to next_as()");
    exit(1);
}

struct u64_hash_table * clone_u64_hash_table(struct u64_hash_table * from, struct lox_allocator * allocator) {
    struct u64_hash_table * new = LOX_MALLOC(allocator, sizeof(struct u64_hash_table));
    new->entries = LOX_MALLOC(from->allocator, sizeof(struct u64_hash_table_entry) * from->capacity);
    memcpy(new->entries, from->entries, sizeof(struct u64_hash_table_entry) * from->capacity);
    new->allocator = from->allocator;
    new->capacity = from->capacity;
    new->size = from->size;
    return new;
}