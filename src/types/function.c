#include "function.h"

struct function_object * alloc_function() {
    struct function_object * function_object = ALLOCATE_OBJ(struct function_object, OBJ_FUNCTION);
    function_object->struct_instances = NULL;
    init_chunk(&function_object->chunk);
    function_object->n_arguments = 0;
    function_object->name = NULL;

    return function_object;
}

struct struct_instance * alloc_struct_compilation_instance() {
    struct struct_instance * instance = malloc(sizeof(struct struct_instance));
    instance->struct_definition = NULL;
    instance->next = NULL;
    return instance;
}