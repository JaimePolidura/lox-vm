#include "chunk.h"

#include <stdlib.h>

struct chunk * alloc_chunk() {
    struct chunk * allocated_chunk = malloc(sizeof(struct chunk));
    init_chunk(allocated_chunk);
    return allocated_chunk;
}

int add_constant_to_chunk(struct chunk * chunk_to_write, lox_value_t constant) {
    write_lox_array(&chunk_to_write->constants, constant);
    return chunk_to_write->constants.in_use - 1;
}

void write_chunk(struct chunk * chunk_to_write, uint8_t byte, int line) {
    if(chunk_to_write->in_use + 1 > chunk_to_write->capacity) {
        const int new_code_capacity = GROW_CAPACITY(chunk_to_write->capacity);
        chunk_to_write->capacity = new_code_capacity;
        chunk_to_write->code = reallocate_array(chunk_to_write->code, sizeof(uint8_t) * new_code_capacity);
        chunk_to_write->lines = reallocate_array(chunk_to_write->lines, sizeof(int) * new_code_capacity);
    }

    const int index_to_write = chunk_to_write->in_use++;
    chunk_to_write->code[index_to_write] = byte;
    chunk_to_write->lines[index_to_write] = line;
}

void free_chunk(struct chunk * chunk_to_free) {
    free(chunk_to_free->code);
    free(chunk_to_free->lines);
    free(&chunk_to_free->constants);
}

void init_chunk(struct chunk * chunk) {
    chunk->capacity = 0;
    chunk->in_use = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    alloc_lox_array(&chunk->constants);
}