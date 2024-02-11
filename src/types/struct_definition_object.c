#include "struct_definition_object.h"

struct struct_definition_object * alloc_struct_definition_object() {
    struct struct_definition_object * struct_object = malloc(sizeof(struct struct_definition_object));
    init_struct_definition_object(struct_object);
    return struct_object;
}

void init_struct_definition_object(struct struct_definition_object * struct_object) {
    struct_object->object.gc_marked = false;
    struct_object->object.type = OBJ_STRUCT_DEFINITION;
    init_hash_table(&struct_object->definition.fields);
    struct_object->definition.n_fields = 0;
    struct_object->definition.is_public = false;
    struct_object->definition.name.start = NULL;
    struct_object->definition.name.length = 0;
}