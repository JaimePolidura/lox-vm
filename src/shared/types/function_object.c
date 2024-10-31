#include "function_object.h"

struct function_object * alloc_function() {
    struct function_object * function_object = ALLOCATE_OBJ(struct function_object, OBJ_FUNCTION);
    function_object->chunk = alloc_chunk();
    function_object->n_arguments = 0;
    function_object->name = NULL;

    for(int i = 0; i < MAX_MONITORS_PER_FUNCTION; i++){
        init_monitor(&function_object->monitors[i]);
    }

    init_object(&function_object->object, OBJ_FUNCTION);

    function_object->state_as.not_profiling.n_calls = 0;

    return function_object;
}