#include "utils.h"

int sizeof_heap_allocated_lox_object(struct object * object) {
    switch (object->type) {
        case OBJ_STRING: return ((struct string_object *) object)->length + 1 + sizeof(struct string_object);
        case OBJ_ARRAY: return sizeof(struct array_object) + sizeof(lox_value_t) * ((struct array_object *) object)->values.in_use;
        case OBJ_STRUCT_INSTANCE: {
            struct struct_instance_object * instance = (struct struct_instance_object *) object;
            struct struct_definition_object * definition = instance->definition;

            return sizeof(struct struct_instance_object) + definition->n_fields * sizeof(lox_value_t);
        }
        default: return 0;
    }
}

void free_heap_allocated_lox_object(struct object * object) {
    switch (object->type) {
        case OBJ_STRING: {
            free(((struct string_object *) object)->chars);
            break;
        }
        case OBJ_STRUCT_INSTANCE: {
            struct struct_instance_object * struct_instance = (struct struct_instance_object *) object;
            free_hash_table(&struct_instance->fields);
            break;
        }
        case OBJ_ARRAY: {
            struct array_object * array_object = (struct array_object *) object;
            free_lox_arraylist(&array_object->values);
            break;
        }
    }

    free(object);
}