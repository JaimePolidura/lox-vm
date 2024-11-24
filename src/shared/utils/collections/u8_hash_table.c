#include "u8_hash_table.h"

struct u8_hash_table * alloc_u8_hash_table(struct lox_allocator * allocator) {
    struct u8_hash_table * table = LOX_MALLOC(allocator, sizeof(struct u8_hash_table));
    init_u8_hash_table(table);
    return table;
}

struct u8_hash_table * clone_u8_hash_table(struct u8_hash_table * to_be_cloned, struct lox_allocator * allocator) {
    struct u8_hash_table * cloned = alloc_u8_hash_table(allocator);
    memcpy(cloned, to_be_cloned, sizeof(struct u8_hash_table));
    return cloned;
}

void init_u8_hash_table(struct u8_hash_table * table) {
    memset(table, 0, sizeof(struct u8_hash_table));
}

void * get_u8_hash_table(struct u8_hash_table * table, uint8_t key) {
    return table->data[key];
}

bool put_u8_hash_table(struct u8_hash_table * table, uint8_t key, void * value) {
    bool doesnt_exist = table->data[key] == NULL;
    if(doesnt_exist){
        table->size++;
    }
    table->data[key] = value;
    return !doesnt_exist;
}

bool contains_u8_hash_table(struct u8_hash_table * table, uint8_t key) {
    return table->data[key] != NULL;
}