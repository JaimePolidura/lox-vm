#pragma once

#include "shared/string_pool.h"
#include "shared/types/types.h"
#include "shared/utils/collections/lox_arraylist.h"
#include "shared.h"
#include "shared/utils/utils.h"

//Represents a chunk of executable pending_bytecode by the vm generated by the compiler
//Each function code will be represented by a chunk
//And the main file code will be represented by a chunk value_as well which contains other chunks (functions)
struct chunk {
    struct lox_arraylist constants;
    uint8_t * code;
    int capacity; //nº of code allocated
    int in_use; //nº of code used
    int * lines; //Mapping of byte code line to line of original lox code
};

struct chunk_bytecode_context {
    uint8_t * code;
    int in_use; //nº of code used
    int capacity; //nº of code allocated
};

void write_chunk(struct chunk * chunk_to_write, uint8_t byte);
int add_constant_to_chunk(struct chunk * chunk_to_write, lox_value_t constant);
struct chunk * alloc_chunk();
void init_chunk(struct chunk * chunk);
void free_chunk(struct chunk * chunk_to_free);
struct chunk * copy_chunk(struct chunk * src);

struct chunk_bytecode_context chunk_start_new_context(struct chunk * chunk);
struct chunk_bytecode_context chunk_restore_context(struct chunk * chunk, struct chunk_bytecode_context prev);
void chunk_write_context(struct chunk * chunk, struct chunk_bytecode_context prev);