#include "struct_definition_object.h"

struct struct_definition_object * alloc_struct_definition_object() {
    struct struct_definition_object * struct_object = ALLOCATE_OBJ(struct struct_definition_object, OBJ_STRUCT_DEFINITION);
    init_struct_definition_object(struct_object);
    return struct_object;
}

void init_struct_definition_object(struct struct_definition_object * struct_object) {
    struct_object->object.type = OBJ_STRUCT_DEFINITION;
    struct_object->n_fields = 0;
    struct_object->is_public = false;
    struct_object->name = NULL;
    init_object(&struct_object->object, OBJ_STRUCT_DEFINITION);
}

uint16_t offset_field_struct_definition_object(
        struct struct_definition_object * definition,
        char * field_name
) {
    int n_fields = definition->n_fields;

}