#pragma once

#include "shared.h"
#include "utils/trie.h"
#include "compiled_function.h"
#include "types/function.h"

typedef enum {
    EXPORTED_SYMBOL_VAR,
    EXPORTED_SYMBOL_FUNCTION,
    EXPORTED_SYMBOL_STRUCT
} exported_symbol_type_t;

struct exported_symbol {
    exported_symbol_type_t type;

    union {
        struct struct_definition * struct_definition;
        struct compiled_function * function_object;
        char * var;
    } as;
};

struct exported_symbol * to_exported_symbol_function(struct compiled_function * compiled_function);
struct exported_symbol * to_exported_symbol_struct(struct struct_definition * struct_definition);
struct exported_symbol * to_exported_symbol_var(struct token name);

int get_name_length_from_symbol(struct exported_symbol * symbol);

char * get_name_char_from_symbol(struct exported_symbol * symbol);
