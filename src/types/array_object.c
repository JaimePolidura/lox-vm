#include "array_object.h"

struct array_object * alloc_array_object() {
    struct array_object * array_object = malloc(sizeof(struct array_object));
    array_object->n_elements = 0;
    array_object->values = NULL;
    init_object(&array_object->object, OBJ_ARRAY);
    return array_object;
}

void empty_initialization(struct array_object * array, int n_elements) {
    array->n_elements = 0;
    array->values = malloc(sizeof(lox_value_t) * n_elements);
    memset(array->values, 0, n_elements);
}