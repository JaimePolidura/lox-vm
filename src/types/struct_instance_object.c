#include "struct_instance_object.h"

struct struct_instance_object * alloc_struct_instance_object() {
    struct struct_instance_object * struct_object = malloc(sizeof(struct struct_instance_object));
    init_struct_instance_object(struct_object);
    return struct_object;
}

void init_struct_instance_object(struct struct_instance_object * struct_object) {
    init_hash_table(&struct_object->fields);
    struct_object->object.gc_marked = false;
    struct_object->object.type = OBJ_STRUCT_INSTANCE;
    struct_object->definition = NULL;
}