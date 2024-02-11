#include "exported_symbol.h"

struct exported_symbol * to_exported_symbol_function(struct function_object * function_object) {
    struct exported_symbol * exported_symbol = malloc(sizeof(struct exported_symbol));
    exported_symbol->as.function_object = function_object;
    exported_symbol->type = EXPORTED_SYMBOL_FUNCTION;
    return exported_symbol;
}

struct exported_symbol * to_exported_symbol_struct(struct struct_definition * struct_definition) {
    struct exported_symbol * exported_symbol = malloc(sizeof(struct exported_symbol));
    exported_symbol->as.struct_definition = struct_definition;
    exported_symbol->type = EXPORTED_SYMBOL_STRUCT;
    return exported_symbol;
}

struct exported_symbol * to_exported_symbol_var(int var_identifier) {
    struct exported_symbol * exported_symbol = malloc(sizeof(struct exported_symbol));
    exported_symbol->as.identifier = var_identifier;
    exported_symbol->type = EXPORTED_SYMBOL_VAR;
    return exported_symbol;
}
