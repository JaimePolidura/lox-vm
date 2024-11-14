#include "u64_set.h"

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
