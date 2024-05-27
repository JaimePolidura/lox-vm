#pragma once

#include "compiler/bytecode/chunk/chunk.h"
#include "shared/bytecode/bytecode.h"

struct chunk_iterator {
    struct chunk * iterating;
    uint8_t * pc;
    bytecode_t last_instruction;
};

struct chunk_iterator iterate_chunk(struct chunk * chunk);

lox_value_t read_constant_chunk_iterator(struct chunk_iterator *);
uint8_t read_u8_chunk_iterator_at(struct chunk_iterator *, int offset_from_bytecode);
uint16_t read_u16_chunk_iterator(struct chunk_iterator *);
uint8_t read_u8_chunk_iterator(struct chunk_iterator *);
int current_instruction_index_chunk_iterator(struct chunk_iterator *);

bytecode_t next_instruction_chunk_iterator(struct chunk_iterator *);
bool has_next_chunk_iterator(struct chunk_iterator *);
