#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared.h"

struct u8_hash_table {
    void* data [UINT8_MAX];
    uint8_t size;
};

void init_u8_hash_table(struct u8_hash_table *);
struct u8_hash_table * alloc_u8_hash_table(struct lox_allocator *);
struct u8_hash_table * clone_u8_hash_table(struct u8_hash_table *, struct lox_allocator *);

void * get_u8_hash_table(struct u8_hash_table *, uint8_t key);
bool put_u8_hash_table(struct u8_hash_table *, uint8_t key, void * value);
bool contains_u8_hash_table(struct u8_hash_table *, uint8_t key);