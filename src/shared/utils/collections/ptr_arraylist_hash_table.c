#include "ptr_arraylist_hash_table.h"

#include "lox_hash_table.h"
#include "ptr_arraylist.h"
#include "u64_hash_table.h"

void init_ptr_arraylist_hash_table(struct ptr_arraylist_hash_table * table, struct lox_allocator * allocator) {
    init_u64_hash_table(&table->hash_table, allocator);
    table->allocator = allocator;
}

struct ptr_arraylist_hash_table * alloc_ptr_arraylist_hash_table(struct lox_allocator * allocator) {
    struct ptr_arraylist_hash_table * ptr_arraylist_hash_table = LOX_MALLOC(allocator, sizeof(struct ptr_arraylist_hash_table));
    init_ptr_arraylist_hash_table(ptr_arraylist_hash_table, allocator);
    return ptr_arraylist_hash_table;
}

void free_ptr_arraylist_hash_table(struct ptr_arraylist_hash_table * ptr_arraylist_hash_table) {
    FOR_EACH_U64_HASH_TABLE_ENTRY(ptr_arraylist_hash_table->hash_table, entry) {
        struct ptr_arraylist * ptr_arraylist_to_free = (struct ptr_arraylist *) entry.value;
        free_ptr_arraylist(ptr_arraylist_to_free);
    }

    free_u64_hash_table(&ptr_arraylist_hash_table->hash_table);
}

void put_ptr_arraylist_hash_table(struct ptr_arraylist_hash_table * ptr_arraylist_hash_table, uint64_t key, void * value) {
    if (!contains_u64_hash_table(&ptr_arraylist_hash_table->hash_table, key)) {
        struct ptr_arraylist * ptr_arraylist = alloc_ptr_arraylist(ptr_arraylist_hash_table->allocator);
        put_u64_hash_table(&ptr_arraylist_hash_table->hash_table, key, ptr_arraylist);
    }

    struct ptr_arraylist * ptr_arraylist = get_u64_hash_table(&ptr_arraylist_hash_table->hash_table, key);
    append_ptr_arraylist(ptr_arraylist, value);
}

struct ptr_arraylist * get_ptr_arraylist_hash_table(struct ptr_arraylist_hash_table * ptr_arraylist_hash_table, uint64_t key) {
    if (!contains_u64_hash_table(&ptr_arraylist_hash_table->hash_table, key)) {
        struct ptr_arraylist * ptr_arraylist = alloc_ptr_arraylist(ptr_arraylist_hash_table->allocator);
        put_u64_hash_table(&ptr_arraylist_hash_table->hash_table, key, ptr_arraylist);
    }

    return get_u64_hash_table(&ptr_arraylist_hash_table->hash_table, key);
}