#include "u64_set.h"

struct u64_set empty_u64_set(struct lox_allocator * allocator) {
    struct u64_set set;
    init_u64_set(&set, allocator);
    return set;
}

void clear_u64_set(struct u64_set * set) {
    clear_u64_hash_table(&set->inner_hash_table);
}

struct u64_set * alloc_u64_set(struct lox_allocator * allocator) {
    struct u64_set * u64_set = LOX_MALLOC(allocator, sizeof(struct u64_set));
    init_u64_set(u64_set, allocator);
    return u64_set;
}

void init_u64_set(struct u64_set * set, struct lox_allocator * allocator) {
    init_u64_hash_table(&set->inner_hash_table, allocator);
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

void difference_u64_set(struct u64_set * a, struct u64_set b) {
    FOR_EACH_U64_SET_VALUE(b, curent_b_value) {
        remove_u64_set(a, curent_b_value);
    }
}

bool is_subset_u64_set(struct u64_set a, struct u64_set b) {
    FOR_EACH_U64_SET_VALUE(b, current_b_value) {
        if (!contains_u64_set(&a, current_b_value)) {
            return false;
        }
    }

    return true;
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

void remove_u64_set(struct u64_set * set, uint64_t value) {
    remove_u64_hash_table(&set->inner_hash_table, value);
}

void union_u64_set(struct u64_set * values, struct u64_set other) {
    struct u64_set_iterator other_it;
    init_u64_set_iterator(&other_it, other);
    while(has_next_u64_set_iterator(other_it)){
        add_u64_set(values, next_u64_set_iterator(&other_it));
    }
}

uint64_t get_first_value_u64_set(struct u64_set set) {
    struct u64_set_iterator iterator;
    init_u64_set_iterator(&iterator, set);
    if(has_next_u64_set_iterator(iterator)){
        return next_u64_set_iterator(&iterator);
    } else {
        return 0;
    }
}