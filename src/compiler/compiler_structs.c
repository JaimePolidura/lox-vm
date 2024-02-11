#include "compiler_structs.h"

struct struct_definition * alloc_struct_definition() {
    struct struct_definition * compiler_struct = malloc(sizeof(struct struct_definition *));
    compiler_struct->field_names = NULL;
    compiler_struct->n_fields = 0;

    return compiler_struct;;
}