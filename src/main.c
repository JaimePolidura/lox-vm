#include <stdio.h>
#include <stdlib.h>

#include "shared.h"
#include "chunk/chunk.h"
#include "chunk/chunk_disassembler.h"
#include "vm/vm.h"
#include "compiler/package_compiler.h"

void debug_simple_calculation();
void prod();
void run_file(char * path);
char * read_file_source_code(char * path);
interpret_result interpret_source_code(char * source_code);

int main(int argc, char* args[]) {
    init_global_string_pool();
    start_vm();

    run_file(args[1]);

    stop_vm();

    return 0;
}

void run_file(char * path) {
    char * source_code = read_file_source_code(path);
    interpret_result result = interpret_source_code(source_code);
    free(source_code);

    switch (result) {
        case INTERPRET_RUNTIME_ERROR: exit(70);
        case INTERPRET_COMPILE_ERROR: exit(65);
    }
}

interpret_result interpret_source_code(char * source_code) {
    struct compilation_result compilation_result = compile(source_code);

    if(!compilation_result.success){
        return INTERPRET_COMPILE_ERROR;
    }

    return interpret_vm(compilation_result);
}

char * read_file_source_code(char * path) {
    FILE * file = fopen(path, "rb");
    if(file == NULL) {
        fprintf(stderr, "Cannot open file %s\n", path);
        exit(74);
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char * source_code_buffer = malloc(file_size + 1);
    if(source_code_buffer == NULL) {
        fprintf(stderr, "Cannot allocate buffer to read file %s\n", path);
        exit(74);
    }

    size_t bytesRead = fread(file, sizeof(char), file_size, source_code_buffer);
    if(bytesRead < file_size) {
        fprintf(stderr, "Cannot read file %s\n", path);
        exit(74);
    }

    source_code_buffer[bytesRead] = '\0';

    fclose(file);

    return source_code_buffer;
}