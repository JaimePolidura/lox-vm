#include "u64_set.h"

struct u64_set * alloc_u64_set() {
    struct u64_set * u64_set = malloc(sizeof(struct u64_set));
    init_u64_set(u64_set);
    return u64_set;
}

void init_u64_set(struct u64_set * set) {
    init_u64_hash_table(&set->inner_hash_table);
}

void free_u64_set(struct u64_set * set) {
    free_u64_hash_table(&set->inner_hash_table);
}

void add_u64_set(struct u64_set * set, uint64_t value) {
    put_u64_hash_table(&set->inner_hash_table, value, (void *) 0x01);
}

bool contains_u64_set(struct u64_set * set, uint64_t value) {
    return contains_u64_hash_table(&set->inner_hash_table, value);
}

uint8_t size_u64_set(struct u64_set set) {
    return set.inner_hash_table.size;
}

void init_u64_set_iterator(struct u64_set_iterator * iterator, struct u64_set set) {
    init_u64_hash_table_iterator(&iterator->inner_hashtable_iterator, set.inner_hash_table);
}

bool has_next_u64_set_iterator(struct u64_set_iterator iterator) {
    return has_next_u64_hash_table_iterator(iterator.inner_hashtable_iterator);
}

uint64_t next_u64_set_iterator(struct u64_set_iterator * iterator) {
    return next_u64_hash_table_iterator(&iterator->inner_hashtable_iterator).key;
}