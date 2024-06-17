#include "struct_instance_object.h"

void init_struct_instance_object(struct struct_instance_object * struct_object, struct struct_definition_object * definition) {
    init_hash_table(&struct_object->fields);
    struct_object->definition = definition;
    init_object(&struct_object->object, OBJ_STRUCT_INSTANCE);
}