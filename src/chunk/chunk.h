#pragma once

#include "memory/string_pool.h"
#include "../types/types.h"
#include "utils/collections/lox/lox_array_list.h"
#include "../shared.h"

struct chunk {
    struct lox_array_list constants;
    uint8_t * code;
    int capacity; //nº of code allocated
    int in_use; //nº of code used
    int * lines; //Mapping of byte code line to line of original lox code
};

struct chunk_bytecode_context {
    uint8_t * code;
    int in_use; //nº of code used
    int capacity; //nº of code allocated
    int * lines;
};

void write_chunk(struct chunk * chunk_to_write, uint8_t byte, int line);
int add_constant_to_chunk(struct chunk * chunk_to_write, lox_value_t constant);
struct chunk * alloc_chunk();
void init_chunk(struct chunk * chunk);
void free_chunk(struct chunk * chunk_to_free);

struct chunk_bytecode_context chunk_start_new_context(struct chunk * chunk);
struct chunk_bytecode_context chunk_restore_context(struct chunk * chunk, struct chunk_bytecode_context prev);
void chunk_write_context(struct chunk * chunk, struct chunk_bytecode_context prev);