#include "struct_definition_object.h"

struct struct_definition_object * alloc_struct_definition_object() {
    struct struct_definition_object * struct_object = malloc(sizeof(struct struct_definition_object));
    init_struct_definition_object(struct_object);
    return struct_object;
}

void init_struct_definition_object(struct struct_definition_object * struct_object) {
    struct_object->object.gc_marked = false;
    struct_object->object.type = OBJ_STRUCT_DEFINITION;
    struct_object->n_fields = 0;
    struct_object->is_public = false;
    struct_object->name = NULL;
}