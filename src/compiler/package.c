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
    init_chunk(&package->chunk);
}

char * read_package_name(struct token import_path) {
    return NULL; //TOOD
}