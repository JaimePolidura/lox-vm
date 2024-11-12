#include "u8_hash_table.h"

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