#include "lox_arraylist.h"

void init_lox_arraylist_with_size(struct lox_arraylist * array, int size, struct lox_allocator * allocator) {
    init_lox_arraylist(array, allocator);
    array->capacity = size;
    array->in_use = size;
    array->values = GROW_ARRAY(allocator, lox_value_t, array->values, 0, size);
    memset(array->values, 0, size * sizeof(lox_value_t));
}

void init_lox_arraylist(struct lox_arraylist * array, struct lox_allocator * allocator) {
    array->allocator = allocator;
    array->values = NULL;
    array->capacity = 0;
    array->in_use = 0;
}

void append_lox_arraylist(struct lox_arraylist * array, lox_value_t value) {
    if(array->in_use + 1 > array->capacity) {
        const int new_capacity = GROW_CAPACITY(array->capacity);
        const int old_capacity = array->capacity;

        array->values = GROW_ARRAY(array->allocator, lox_value_t, array->values, old_capacity, new_capacity);
        array->capacity = new_capacity;
    }

    array->values[array->in_use++] = value;
}

void flip_lox_arraylist(struct lox_arraylist * array) {
    for(int i = 0; i < array->in_use / 2; i++){
        int a_index = i;
        int b_index = array->in_use - a_index - 1;

        lox_value_t a_value = array->values[a_index];
        lox_value_t b_value = array->values[b_index];

        array->values[a_index] = b_value;
        array->values[b_index] = a_value;
    }
}

void free_lox_arraylist(struct lox_arraylist * array){
    free(array->values);
}