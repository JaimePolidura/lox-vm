#include "exported_symbol.h"

struct exported_symbol * alloc_exported_symbol(int identifier, bool is_const, exported_symbol_type_t type) {
    struct exported_symbol * exported_symbol = malloc(sizeof(struct exported_symbol));
    exported_symbol->constant_identifier = identifier;
    exported_symbol->is_const = is_const;
    exported_symbol->type = type;
    return exported_symbol;
}