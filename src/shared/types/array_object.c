#include "array_object.h"

bool set_element_array(struct array_object * array, int index, lox_value_t value) {
    if(index >= array->values.capacity){
        return false;
    }

    array->values.values[index] = value;

    return true;
}