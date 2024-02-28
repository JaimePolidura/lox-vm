#include "array_object.h"

struct array_object * alloc_array_object(int size) {
    struct array_object * array_object = malloc(sizeof(struct array_object));
    init_lox_arraylist_with_size(&array_object->values, size);
    init_object(&array_object->object, OBJ_ARRAY);
    return array_object;
}

bool set_element_array(struct array_object * array, int index, lox_value_t value) {
    if(index >= array->values.capacity){
        return false;
    }

    array->values.values[index] = value;

    return true;
}