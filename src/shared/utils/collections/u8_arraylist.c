#include "u8_arraylist.h"

void init_u8_arraylist(struct u8_arraylist * array) {
    array->values = NULL;
    array->capacity = 0;
    array->in_use = 0;
}

void free_u8_arraylist(struct u8_arraylist * array) {
    free(array->values);
}

uint16_t append_u8_arraylist(struct u8_arraylist * array, uint8_t value) {
    if(array->in_use + 1 > array->capacity) {
        const int new_capacity = GROW_CAPACITY(array->capacity);
        const int old_capacity = array->capacity;

        array->values = GROW_ARRAY(uint8_t, array->values, old_capacity, new_capacity);
        array->capacity = new_capacity;
    }

    array->values[array->in_use++] = value;

    return array->in_use - 1;
}