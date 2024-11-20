#include "ptr_arraylist.h"

struct ptr_arraylist * alloc_ptr_arraylist() {
    struct ptr_arraylist * ptr_arraylist = malloc(sizeof(struct ptr_arraylist));
    init_ptr_arraylist(ptr_arraylist);
    return ptr_arraylist;
}

void init_ptr_arraylist(struct ptr_arraylist * array) {
    array->values = NULL;
    array->capacity = 0;
    array->in_use = 0;
}

void free_ptr_arraylist(struct ptr_arraylist * array) {
    free(array->values);
}

uint16_t append_ptr_arraylist(struct ptr_arraylist * array, void * value) {
    if(array->in_use + 1 > array->capacity) {
        const int new_capacity = GROW_CAPACITY(array->capacity);
        const int old_capacity = array->capacity;
        array->values = GROW_ARRAY(void *, array->values, old_capacity, new_capacity);
        array->capacity = new_capacity;
    }

    array->values[array->in_use++] = value;

    return array->in_use - 1;
}

void resize_ptr_arraylist(struct ptr_arraylist * array, int new_size) {
    if(array->capacity < new_size){
        array->values = GROW_ARRAY(void * , array->values, array->capacity, new_size);
        array->capacity = new_size;
    }
}