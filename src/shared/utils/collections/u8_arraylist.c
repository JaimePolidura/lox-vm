#include "u8_arraylist.h"

void init_u8_arraylist(struct u8_arraylist * array, struct lox_allocator * allocator) {
    array->allocator = allocator;
    array->values = NULL;
    array->capacity = 0;
    array->in_use = 0;
}

void free_u8_arraylist(struct u8_arraylist * array) {
    LOX_FREE(array->allocator, array->values);
}

uint16_t append_u8_arraylist(struct u8_arraylist * array, uint8_t value) {
    if(array->in_use + 1 > array->capacity) {
        const int new_capacity = GROW_CAPACITY(array->capacity);
        const int old_capacity = array->capacity;

        array->values = GROW_ARRAY(array->allocator, uint8_t, array->values, old_capacity, new_capacity);
        array->capacity = new_capacity;
    }

    array->values[array->in_use++] = value;

    return array->in_use - 1;
}