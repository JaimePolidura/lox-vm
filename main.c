#include <stdio.h>
#include <stdlib.h>

#include "src/shared.h"
#include "src/chunk/chunk.h"
#include "src/chunk/chunk_disassembler.h"
#include "src/vm/vm.h"
#include "src/compiler/compiler.h"

void debug_simple_calculation();
void prod();
void run_file(char * path);
char * read_file_source_code(char * path);
interpret_result interpret_source_code(char * source_code);

int main(int argc, char* args[]) {
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
    struct chunk * chunk = alloc_chunk();
    bool compilation_success = compile(source_code, chunk);

    if(!compilation_success){
        return INTERPRET_COMPILE_ERROR;
    }

    return interpret_vm(chunk);
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

void debug_simple_calculation() {
    start_vm();
    struct chunk * chunk = alloc_chunk();

    // -((1.2 + 3.4) / 5.6)
    write_chunk(chunk, OP_CONSTANT, 1);
    write_chunk(chunk, add_constant_to_chunk(chunk, FROM_NUMBER(1.2)), 1);
    write_chunk(chunk, OP_CONSTANT, 1);
    write_chunk(chunk, add_constant_to_chunk(chunk, FROM_NUMBER(3.4)), 1);
    write_chunk(chunk, OP_ADD, 1);
    write_chunk(chunk, OP_CONSTANT, 1);
    write_chunk(chunk, add_constant_to_chunk(chunk, FROM_NUMBER(5.6)), 1);
    write_chunk(chunk, OP_DIV, 1);
    write_chunk(chunk, OP_NEGATE, 1);
    write_chunk(chunk, OP_RETURN, 1);
    write_chunk(chunk, OP_EOF, 1); //Expect -0.81

    interpret_vm(chunk);

    stop_vm();
    free_chunk(chunk);
}

void prod() {
    start_vm();



    stop_vm();
}