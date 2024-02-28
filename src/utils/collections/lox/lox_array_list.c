#include "lox_array_list.h"

void init_lox_array(struct lox_array_list * array) {
    array->values = NULL;
    array->capacity = 0;
    array->in_use = 0;
}

void write_lox_array(struct lox_array_list * array, lox_value_t value) {
    if(array->in_use + 1 > array->capacity) {
        const int new_capacity = GROW_CAPACITY(array->capacity);
        const int old_capacity = array->capacity;

        array->values = GROW_ARRAY(lox_value_t, array->values, old_capacity, new_capacity);
        array->capacity = new_capacity;
    }

    array->values[array->in_use++] = value;
}

void flip_lox_array(struct lox_array_list * array) {
    for(int i = 0; i < array->in_use / 2; i++){
        int a_index = i;
        int b_index = array->in_use - a_index - 1;

        lox_value_t a_value = array->values[a_index];
        lox_value_t b_value = array->values[b_index];

        array->values[a_index] = b_value;
        array->values[b_index] = a_value;
    }
}

void free_lox_array(struct lox_array_list * array){
    free(array->values);
}