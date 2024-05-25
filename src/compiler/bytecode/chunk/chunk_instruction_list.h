#pragma once

#include "compiler/bytecode/chunk/chunk_iterator.h"
#include "compiler/bytecode/chunk/chunk.h"
#include "compiler/bytecode/bytecode.h"
#include "shared.h"

struct chunk_instruction_list {
    bytecode_t bytecode;
    union {
        uint8_t u8;
        uint16_t u16;
        struct {
            uint8_t u8_1;
            uint8_t u8_2;
        } pair;
    } as;

    struct chunk_instruction_list * next;
};

struct chunk_instruction_list * create_chunk_instruction_list(struct chunk *);
struct chunk * to_chunk_instruction_list(struct chunk_instruction_list *);
void free_chunk_instruction_list(struct chunk_instruction_list *);