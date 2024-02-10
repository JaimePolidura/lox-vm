#include "exported_symbol.h"

struct exported_symbol * to_exported_symbol_function(struct compiled_function * compiled_function) {
    struct exported_symbol * exported_symbol = malloc(sizeof(struct exported_symbol));
    exported_symbol->as.function_object = compiled_function;
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
    exported_symbol->as.var_identifier = var_identifier;
    exported_symbol->type = EXPORTED_SYMBOL_VAR;
    return exported_symbol;
}

int get_name_length_from_symbol(struct exported_symbol * symbol) {
    switch (symbol->type) {
        case EXPORTED_SYMBOL_FUNCTION: return symbol->as.function_object->function_object->name->length;
        default: return -1;
    }
}

char * get_name_char_from_symbol(struct exported_symbol * symbol) {
    switch (symbol->type) {
        case EXPORTED_SYMBOL_FUNCTION: return symbol->as.function_object->function_object->name->chars;
        default: return NULL;
    }
}