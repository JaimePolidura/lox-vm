#include "package.h"

struct package * alloc_package() {
    struct package * package = malloc(sizeof(struct package));
    init_package(package);
    return package;
}

void init_package(struct package * package) {
    init_trie_list(&package->struct_definitions);
    init_trie_list(&package->exported_symbols);
    package->name = NULL;
    package->state = PENDING_COMPILATION;
}

char * read_package_name(struct token import_path) {
    return NULL; //TODO
}

char * read_package_name_by_source_code(char * source_code) {
    return ""; //TODO
}