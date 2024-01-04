#pragma once

#include "../types/strings/string_pool.h"
#include "../memory/memory.h"
#include "../types/types.h"
#include "../types/array.h"
#include "../shared.h"

struct chunk {
    struct lox_array constants;
    uint8_t * code;
    int capacity; //nº of code allocated
    int in_use; //nº of code used
    int * lines;
    struct string_pool compiled_string_pool;
};

void write_chunk(struct chunk * chunk_to_write, uint8_t byte, int line);
int add_constant_to_chunk(struct chunk * chunk_to_write, lox_value_t constant);
struct chunk * alloc_chunk();
void init_chunk(struct chunk * chunk);
void free_chunk(struct chunk * chunk_to_free);