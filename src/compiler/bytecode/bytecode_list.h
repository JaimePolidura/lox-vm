#pragma once

#include "compiler/bytecode/chunk/chunk_iterator.h"
#include "compiler/bytecode/chunk/chunk.h"
#include "compiler/bytecode/bytecode.h"
#include "shared.h"

struct bytecode_list {
    bytecode_t bytecode;
    union {
        uint8_t u8;
        uint16_t u16;
        struct {
            uint8_t u8_1;
            uint8_t u8_2;
        } pair;
        struct bytecode_list * jump;
    } as;

    struct bytecode_list * next;
    struct bytecode_list * prev;
};

struct bytecode_list * create_bytecode_list(struct chunk *);
struct bytecode_list * alloc_bytecode_list();
struct chunk * to_chunk_bytecode_list(struct bytecode_list *);
void free_bytecode_list(struct bytecode_list *);

struct bytecode_list * get_by_index_bytecode_list(struct bytecode_list *, int target_index);
void unlink_instruciton_bytecode_list(struct bytecode_list * instruction);
void add_instruction_bytecode_list(struct bytecode_list * dst, struct bytecode_list * instruction);
void add_instructions_bytecode_list(struct bytecode_list * dst, struct bytecode_list * instructions);