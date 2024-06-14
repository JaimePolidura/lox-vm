#pragma once

#include "shared.h"
#include "shared/utils/collections/trie.h"
#include "shared/types/function_object.h"

typedef enum {
    EXPORTED_FUNCTION,
    EXPORTED_STRUCT,
    EXPORTED_VAR,
} exported_symbol_type_t;

//When we compile a package variable, function or struct and is marked with "pub", we store struct exported_symbol in package.h
//This is used to check if a package can access other package's symbols
struct exported_symbol {
    exported_symbol_type_t type;
    int constant_identifier;
};

struct exported_symbol * to_exported_symbol(int identifier, exported_symbol_type_t type);
