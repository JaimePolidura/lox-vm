#include "struct_instance_object.h"

struct struct_instance_object * alloc_struct_instance_object() {
    struct struct_instance_object * struct_object = ALLOCATE_OBJ(struct struct_instance_object, OBJ_STRUCT_INSTANCE);
    init_struct_instance_object(struct_object);
    return struct_object;
}

void init_struct_instance_object(struct struct_instance_object * struct_object) {
    init_hash_table(&struct_object->fields);
    struct_object->definition = NULL;
    init_object(&struct_object->object, OBJ_STRUCT_INSTANCE);
}