#include "compiler.h"

struct compilation_result compile(char * entrypoint_absolute_path, char * compilation_base_dir, char * package_name_entrypoint) {
    char * source_code = read_package_source_code(entrypoint_absolute_path);

    if(source_code == NULL) {
        return (struct compilation_result) {
                .compiled_package = NULL,
                .error_message = "Cannot read entrypoint file",
                .success = false
        };
    }

    struct compilation_result bytecode_compilation = compile_bytecode(source_code, package_name_entrypoint, compilation_base_dir);

    free(source_code);

    return bytecode_compilation;
}

struct compilation_result compile_standalone(char * source_code) {
    struct compilation_result bytecode_compilation = compile_bytecode(source_code, "main", NULL);

    return bytecode_compilation;
}