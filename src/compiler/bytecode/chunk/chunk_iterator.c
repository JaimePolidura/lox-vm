#include "chunk_iterator.h"

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

    chunk_iterator->pc += get_size_bytecode_instruction(current_instruction) - 1;
    chunk_iterator->last_instruction = current_instruction;

    return current_instruction;
}

uint8_t read_u8_chunk_iterator(struct chunk_iterator * chunk_iterator) {
    return *(chunk_iterator->pc - 1);
}

uint8_t read_u8_chunk_iterator_at(struct chunk_iterator * chunk_iterator, int offset_from_bytecode) {
    int last_instruction_size = get_size_bytecode_instruction(chunk_iterator->last_instruction);
    uint8_t * last_instruction_ptr = chunk_iterator->pc - last_instruction_size;

    return *(last_instruction_ptr + offset_from_bytecode + 1);
}

uint16_t read_u16_chunk_iterator(struct chunk_iterator * chunk_iterator) {
    uint8_t first_half = *(chunk_iterator->pc - 2);
    uint8_t second_half = *(chunk_iterator->pc - 1);
    return first_half << 8 | second_half;
}

int current_instruction_index_chunk_iterator(struct chunk_iterator * chunk_iterator) {
    int current_pc_index = chunk_iterator->pc - chunk_iterator->iterating->code;
    int last_instruction_size = get_size_bytecode_instruction(chunk_iterator->last_instruction);

    return current_pc_index - last_instruction_size;
}

bool has_next_chunk_iterator(struct chunk_iterator * chunk_iterator) {
    if(chunk_iterator->eof_already_hitted){
        return false;
    }
    if(*chunk_iterator->pc == OP_EOF){
        chunk_iterator->eof_already_hitted = true;
    }

    return true;
}
