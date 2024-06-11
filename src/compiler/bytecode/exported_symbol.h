#pragma once

#include "shared.h"
#include "shared/utils/collections/trie.h"
#include "shared/types/function_object.h"

typedef enum {
    EXPORTED_FUNCTION,
    EXPORTED_STRUCT,
    EXPORTED_VAR,
} exported_symbol_type_t;

struct exported_symbol {
    exported_symbol_type_t type;
    int constant_identifier;
};

struct exported_symbol * to_exported_symbol(int identifier, exported_symbol_type_t type);