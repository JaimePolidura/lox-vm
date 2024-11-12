#pragma once

#include "shared.h"

struct u8_hash_table {
    void* data [UINT8_MAX];
    uint8_t size;
};

void init_u8_hash_table(struct u8_hash_table *);

void * get_u8_hash_table(struct u8_hash_table *, uint8_t key);

bool put_u8_hash_table(struct u8_hash_table *, uint8_t key, void * value);

bool contains_u8_hash_table(struct u8_hash_table *, uint8_t key);