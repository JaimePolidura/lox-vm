#include "chunk_iterator.h"

static int get_instruction_size(bytecode_t instruction);

struct chunk_iterator iterate_chunk(struct chunk * chunk) {
    return (struct chunk_iterator) {
        .iterating = chunk,
        .pc = chunk->code
    };
}

lox_value_t read_constant_chunk_iterator(struct chunk_iterator * chunk_iterator) {
    return chunk_iterator->iterating->constants.values[read_u8_chunk_iterator(chunk_iterator)];
}

bytecode_t next_instruction_chunk_iterator(struct chunk_iterator * chunk_iterator) {
    bytecode_t current_instruction = *chunk_iterator->pc++;

    chunk_iterator->pc += instruction_bytecode_length(current_instruction) - 1;
    chunk_iterator->last_instruction = current_instruction;

    return current_instruction;
}

uint8_t read_u8_chunk_iterator(struct chunk_iterator * chunk_iterator) {
    return *(chunk_iterator->pc - 1);
}

uint8_t read_u8_chunk_iterator_at(struct chunk_iterator * chunk_iterator, int offset_from_bytecode) {
    int last_instruction_size = instruction_bytecode_length(chunk_iterator->last_instruction);
    uint8_t * last_instruction_ptr = chunk_iterator->pc - last_instruction_size;

    return *(last_instruction_ptr + offset_from_bytecode + 1);
}

uint16_t read_u16_chunk_iterator(struct chunk_iterator * chunk_iterator) {
    uint8_t first_half = *(chunk_iterator->pc - 2);
    uint8_t second_half = *(chunk_iterator->pc - 1);
    return first_half << 8 | second_half;
}

bool has_next_chunk_iterator(struct chunk_iterator * chunk_iterator) {
    return *chunk_iterator->pc != OP_EOF;
}
