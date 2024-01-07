#include "function.h"

struct function_object * alloc_function() {
    struct function_object * function_object = ALLOCATE_OBJ(struct function_object, OBJ_FUNCTION);
    function_object->arity = 0;
    function_object->name = NULL;
    init_chunk(function_object->chunk);

    return function_object;
}
