#include "exported_symbol.h"

struct exported_symbol * to_exported_symbol_function(struct compiled_function * compiled_function) {
    struct exported_symbol * exported_symbol = malloc(sizeof(struct exported_symbol));
     exported_symbol->type = EXPORTED_SYMBOL_FUNCTION;
     exported_symbol->as.function = compiled_function;
     return exported_symbol;
}

int get_name_length_from_symbol(struct exported_symbol * symbol) {
    switch (symbol->type) {
        case EXPORTED_SYMBOL_FUNCTION: return symbol->as.function->function_object->name->length;
        default: return -1;
    }
}

char * get_name_char_from_symbol(struct exported_symbol * symbol) {
    switch (symbol->type) {
        case EXPORTED_SYMBOL_FUNCTION: return symbol->as.function->function_object->name->chars;
        default: return NULL;
    }
}