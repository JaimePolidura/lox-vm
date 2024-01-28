#include "struct_object.h"

struct struct_object * alloc_struct_object() {
    struct struct_object * struct_object = malloc(sizeof(struct struct_object));
    init_struct_object(struct_object);
    return struct_object;
}

void init_struct_object(struct struct_object * struct_object) {
    struct_object->object.gc_marked = false;
    struct_object->object.type = OBJ_STRUCT;
    init_lox_array(&struct_object->fields);
}