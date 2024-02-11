#include "function.h"

struct function_object * alloc_function() {
    struct function_object * function_object = ALLOCATE_OBJ(struct function_object, OBJ_FUNCTION);
    init_chunk(&function_object->chunk);
    function_object->n_arguments = 0;
    function_object->name = NULL;
    return function_object;
}