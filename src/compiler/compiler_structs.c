#include "compiler_structs.h"

struct struct_definition * alloc_compiler_struct() {
    struct struct_definition * compiler_struct = malloc(sizeof(struct struct_definition *));
    compiler_struct->field_names = NULL;
    compiler_struct->n_fields = 0;
    compiler_struct->next = NULL;

    return compiler_struct;
}