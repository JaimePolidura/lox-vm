#include "package.h"

extern const char * compiling_base_dir;

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
    package->main_function = NULL;
    package->absolute_path = NULL;
    package->package_id = 0;
    pthread_mutex_init(&package->state_mutex, NULL);
}

struct function_object * get_function_package(struct package * package, char * function_name) {
    struct string_object * function_name_string_object = copy_chars_to_string_object(function_name, strlen(function_name));
    struct function_object * function = NULL;

    if(contains_hash_table(&package->defined_functions, function_name_string_object)) {
        lox_value_t value;
        get_hash_table(&package->defined_functions, function_name_string_object, &value);
        function = (struct function_object *) AS_OBJECT(value);
    }

    free(function_name_string_object->chars);
    free(function_name_string_object);

    return function;
}

char * read_package_source_code(char * absolute_path) {
    FILE* file = fopen(absolute_path, "r"); // Open file in read mode
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char * content = (char*) malloc(file_size + 1); // +1 for null terminator
    if (content == NULL) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(content, 1, file_size, file);
    if (bytes_read == 0) {
        fclose(file);
        free(content);
        return NULL;
    }

    content[bytes_read] = '\0';

    fclose(file);

    return content;
}

struct substring read_package_name(char * import_name, int import_name_length) {
    int end = import_name_length;
    int start = 0;

    for(int i = 0; i < end; i++){
        switch (import_name[i]) {
            case '/': start = i + 1; break;
            case '.': end = i; break;
        }
    }

    return (struct substring) {
        .source = import_name,
        .start_inclusive = start,
        .end_exclusive = end,
    };
}

char * import_name_to_absolute_path(char * import_name, int import_name_length) {
    bool is_path = string_contains(import_name, import_name_length, '.');

    if (is_path) {
        char * ptr = malloc(strlen(compiling_base_dir) + strlen(import_name) + 2);
        memcpy(ptr, compiling_base_dir, strlen(compiling_base_dir));
        ptr[strlen(compiling_base_dir)] = '/';
        memcpy(ptr + strlen(compiling_base_dir) + 1, import_name, import_name_length);
        ptr[strlen(compiling_base_dir) + import_name_length + 1] = 0x00;

        return ptr;
    } else {
        //TODO Return path where standard library will be located
        return NULL;
    }
}
