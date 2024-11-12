#include "u8_hash_table.h"

struct u8_hash_table * alloc_u8_hash_table() {
    struct u8_hash_table * table = malloc(sizeof(struct u8_hash_table));
    init_u8_hash_table(table);
    return table;
}

struct u8_hash_table * clone_u8_hash_table(struct u8_hash_table * to_be_cloned) {
    struct u8_hash_table * cloned = alloc_u8_hash_table();
    memcpy(cloned, to_be_cloned, sizeof(sizeof(struct u8_hash_table)));
    return cloned;
}

void init_u8_hash_table(struct u8_hash_table * table) {
    memset(table, 0, sizeof(struct u8_hash_table));
}

void * get_u8_hash_table(struct u8_hash_table * table, uint8_t key) {
    return table->data[key];
}

bool put_u8_hash_table(struct u8_hash_table * table, uint8_t key, void * value) {
    bool already_exists = table->data[key] != NULL;
    if(!already_exists){
        table->size++;
    }
    table->data[key] = value;
    return already_exists;
}

bool contains_u8_hash_table(struct u8_hash_table * table, uint8_t key) {
    return table->data[key] != NULL;
}