#include "ptr_arraylist.h"

struct ptr_arraylist * alloc_ptr_arraylist(struct lox_allocator * allocator) {
    struct ptr_arraylist * ptr_arraylist = LOX_MALLOC(allocator, sizeof(struct ptr_arraylist));
    init_ptr_arraylist(ptr_arraylist, allocator);
    return ptr_arraylist;
}

void init_ptr_arraylist(struct ptr_arraylist * array, struct lox_allocator * allocator) {
    array->allocator = allocator;
    array->values = NULL;
    array->capacity = 0;
    array->in_use = 0;
}

void free_ptr_arraylist(struct ptr_arraylist * array) {
    LOX_FREE(array->allocator, array->values);
}

uint16_t append_ptr_arraylist(struct ptr_arraylist * array, void * value) {
    if(array->in_use + 1 > array->capacity) {
        const int new_capacity = GROW_CAPACITY(array->capacity);
        const int old_capacity = array->capacity;
        array->values = GROW_ARRAY(array->allocator, void *, array->values, old_capacity, new_capacity);
        array->capacity = new_capacity;
    }

    array->values[array->in_use++] = value;

    return array->in_use - 1;
}

void resize_ptr_arraylist(struct ptr_arraylist * array, int new_size) {
    if(array->capacity < new_size){
        array->values = GROW_ARRAY(array->allocator, void * , array->values, array->capacity, new_size);
        array->capacity = new_size;
    }
}

void clear_ptr_arraylist(struct ptr_arraylist * ptr_arraylist) {
    ptr_arraylist->capacity = 0;
    ptr_arraylist->in_use = 0;
    LOX_FREE(ptr_arraylist->allocator, ptr_arraylist->values);
    ptr_arraylist->values = NULL;
}