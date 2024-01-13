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
        const int old_code_capacity = chunk_to_write->capacity;
        chunk_to_write->capacity = new_code_capacity;

        chunk_to_write->code = GROW_ARRAY(uint8_t, chunk_to_write->code, old_code_capacity, new_code_capacity);
        chunk_to_write->lines = GROW_ARRAY(int, chunk_to_write->lines, old_code_capacity, new_code_capacity);
    }

    const int index_to_write = chunk_to_write->in_use++;
    chunk_to_write->code[index_to_write] = byte;
    chunk_to_write->lines[index_to_write] = line;
}

void init_chunk(struct chunk * chunk) {
    alloc_lox_array(&chunk->constants);
    chunk->lines = NULL;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->in_use = 0;
}