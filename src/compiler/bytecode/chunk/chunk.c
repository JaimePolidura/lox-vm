#include "chunk.h"

#include <stdlib.h>

struct chunk * alloc_chunk() {
    struct chunk * allocated_chunk = malloc(sizeof(struct chunk));
    init_chunk(allocated_chunk);
    return allocated_chunk;
}

int add_constant_to_chunk(struct chunk * chunk_to_write, lox_value_t constant) {
    append_lox_arraylist(&chunk_to_write->constants, constant);
    return chunk_to_write->constants.in_use - 1;
}

void write_chunk(struct chunk * chunk_to_write, uint8_t byte) {
    if(chunk_to_write->in_use + 1 > chunk_to_write->capacity) {
        int new_code_capacity = GROW_CAPACITY(chunk_to_write->capacity);
        int old_code_capacity = chunk_to_write->capacity;
        chunk_to_write->capacity = new_code_capacity;

        chunk_to_write->code = GROW_ARRAY(uint8_t, chunk_to_write->code, old_code_capacity, new_code_capacity);
        chunk_to_write->lines = GROW_ARRAY(int, chunk_to_write->lines, old_code_capacity, new_code_capacity);
    }

    int index_to_write = chunk_to_write->in_use++;
    chunk_to_write->code[index_to_write] = byte;
}

void init_chunk(struct chunk * chunk) {
    init_lox_arraylist(&chunk->constants);
    chunk->lines = NULL;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->in_use = 0;
}

struct chunk_bytecode_context chunk_start_new_context(struct chunk * chunk) {
    struct chunk_bytecode_context prev = {
            .code = chunk->code,
            .in_use = chunk->in_use,
            .capacity = chunk->capacity,
    };

    chunk->code = NULL;
    chunk->in_use = 0;
    chunk->capacity = 0;
    chunk->lines = NULL;

    return prev;
}

struct chunk_bytecode_context chunk_restore_context(struct chunk * chunk, struct chunk_bytecode_context prev) {
    struct chunk_bytecode_context actual = {
            .code = chunk->code,
            .in_use = chunk->in_use,
            .capacity = chunk->capacity,
    };

    chunk->code = prev.code;
    chunk->in_use = prev.in_use;
    chunk->capacity = prev.capacity;

    return actual;
}

struct chunk * copy_chunk(struct chunk * src) {
    struct chunk * dst = alloc_chunk();
    dst->capacity = src->capacity;
    dst->in_use = src->in_use;
    dst->lines = src->lines;

    dst->code = malloc(sizeof(uint8_t) * src->capacity);
    memcpy(dst->code, src->code, src->capacity);

    dst->constants.values = malloc(sizeof(lox_value_t) * src->constants.capacity);
    memcpy(dst->constants.values, src->constants.values, dst->constants.capacity);
    dst->constants.capacity = src->constants.capacity;
    dst->constants.in_use = src->constants.in_use;

    return dst;
}

void chunk_write_context(struct chunk * chunk, struct chunk_bytecode_context prev) {
    for(int i = 0; i < prev.in_use; i++){
        write_chunk(chunk, prev.code[i]);
    }

    free(prev.code);
}