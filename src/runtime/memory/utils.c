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

void * grow_array(struct lox_allocator * allocator, size_t new_size, void * original_array, size_t old_size) {
    void * new_array = malloc((sizeof(void *)) * new_size);

    memset(new_array, 0, new_size);

    if(original_array != NULL) {
        memcpy(new_array, original_array, old_size);
        allocator->lox_free(allocator, original_array);
    }

    return new_array;
}

char * dynamic_format_string(const char * format, ...) {
    va_list args;
    va_start(args, format);
    int length = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (length < 0) {
        return NULL;
    }

    char * formatted_string = (char *) malloc((length + 1) * sizeof(char));
    va_start(args, format);
    vsnprintf(formatted_string, length + 1, format, args);
    va_end(args);

    return formatted_string;
}