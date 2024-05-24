#include "exported_symbol.h"

struct exported_symbol * to_exported_symbol(int identifier, exported_symbol_type_t type) {
    struct exported_symbol * exported_symbol = malloc(sizeof(struct exported_symbol));
    exported_symbol->constant_identifier = identifier;
    exported_symbol->type = type;
    return exported_symbol;
}