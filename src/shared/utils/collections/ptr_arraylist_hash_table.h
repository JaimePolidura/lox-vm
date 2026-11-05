#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared.h"
#include "u64_hash_table.h"

struct ptr_arraylist_hash_table {
    struct u64_hash_table hash_table;
    struct lox_allocator * allocator;
};

void init_ptr_arraylist_hash_table(struct ptr_arraylist_hash_table *, struct lox_allocator *);
struct ptr_arraylist_hash_table * alloc_ptr_arraylist_hash_table(struct lox_allocator *);
void free_ptr_arraylist_hash_table(struct ptr_arraylist_hash_table *);

void put_ptr_arraylist_hash_table(struct ptr_arraylist_hash_table*,uint64_t,void*);
struct ptr_arraylist * get_ptr_arraylist_hash_table(struct ptr_arraylist_hash_table*,uint64_t);
