#include "compiler_structs.h"

struct compiler_struct * alloc_compiler_struct() {
    struct compiler_struct * compiler_struct = malloc(sizeof(struct compiler_struct *));
    compiler_struct->field_names = NULL;
    compiler_struct->n_fields = 0;
    compiler_struct->next = NULL;

    return compiler_struct;
}