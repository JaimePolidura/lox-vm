#pragma once

#include "compiler/bytecode/chunk/chunk_iterator.h"
#include "compiler/bytecode/chunk/chunk.h"

#include "shared/bytecode/pending_jump_to_resolve.h"
#include "shared/utils/memory/lox_allocator.h"
#include "shared/bytecode/bytecode.h"
#include "shared.h"

//This is a linked list of pending_bytecode instructions stored in a chunk
//This is used to simplify pending_bytecode transformations
struct bytecode_list {
    bytecode_t bytecode;
    union {
        uint8_t u8;
        uint16_t u16;
        uint64_t u64;
        struct {
            uint8_t u8_1;
            uint8_t u8_2;
        } pair;
        //When building bytecode_list from chunk, jumps are automaticly resolved
        struct bytecode_list * jump;
        struct {
            uint16_t n_elements;
            bool is_emtpy_initializaion;
        } initialize_array;
    } as;

    struct bytecode_list * next;
    struct bytecode_list * prev;

    //Every OP_LOOP will point to a OP_JUMP_IF_FALSE
    bool loop_condition;

    //Orignal pending_bytecode index to chunk
    int original_chunk_index;

    //Only used when converting struct bytecode_list to struct chunk
    int to_chunk_index;

    struct lox_allocator * allocator;
};

struct bytecode_list * create_bytecode_list(struct chunk *, struct lox_allocator *);
struct bytecode_list * create_instruction_bytecode_list(bytecode_t bytecode, struct lox_allocator *);
struct bytecode_list * alloc_bytecode_list(struct lox_allocator *);
struct chunk * to_chunk_bytecode_list(struct bytecode_list *);
void free_bytecode_list(struct bytecode_list *);

struct bytecode_list * get_by_index_bytecode_list(struct bytecode_list *, int target_index);
void unlink_instruction_bytecode_list(struct bytecode_list * instruction);
void add_instruction_bytecode_list(struct bytecode_list * dst, struct bytecode_list * instruction);
void add_instructions_bytecode_list(struct bytecode_list * dst, struct bytecode_list * instructions);
void add_first_instruction_bytecode_list(struct bytecode_list * head, struct bytecode_list * instruction);
void add_last_instruction_bytecode_list(struct bytecode_list * head, struct bytecode_list * instruction);
struct bytecode_list * get_first_bytecode_list(struct bytecode_list * head);
struct bytecode_list * get_next_bytecode_list(struct bytecode_list *, bytecode_t);