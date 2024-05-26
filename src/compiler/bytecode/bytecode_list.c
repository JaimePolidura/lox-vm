#include "bytecode_list.h"

static struct bytecode_list * get_last_instruction(struct bytecode_list *instruction);

struct bytecode_list * alloc_bytecode_list() {
    struct bytecode_list * bytecode_list = malloc(sizeof(struct bytecode_list));
    bytecode_list->bytecode = 0;
    bytecode_list->next = NULL;
    bytecode_list->prev = NULL;
    return bytecode_list;
}

void free_bytecode_list(struct bytecode_list * bytecode_list) {
    struct bytecode_list * current_instruction = bytecode_list;
    while(current_instruction != NULL) {
        struct bytecode_list * next = current_instruction->next;
        free(current_instruction);
        current_instruction = next;
    }
}

struct chunk * to_chunk_bytecode_list(struct bytecode_list * bytecode_list) {
    struct chunk * new_chunk = alloc_chunk();

    struct bytecode_list * current_instruction = bytecode_list;
    while(current_instruction != NULL) {
        int instruction_length = get_size_bytecode_instruction(current_instruction->bytecode);

        write_chunk(new_chunk, current_instruction->bytecode);

        if (instruction_length == 2) {
            write_chunk(new_chunk, current_instruction->as.u8);
        } else if(instruction_length == 3) {
            write_chunk(new_chunk, current_instruction->as.pair.u8_1);
            write_chunk(new_chunk, current_instruction->as.pair.u8_2);
        }

        current_instruction = current_instruction->next;
    }

    return new_chunk;
}

void add_instructions_bytecode_list(struct bytecode_list * dst, struct bytecode_list * instructions) {
    struct bytecode_list * src_most_next = get_last_instruction(instructions);
    struct bytecode_list * next_dst = dst->next;

    dst->next = instructions;
    instructions->prev = dst;

    if(next_dst != NULL){
        next_dst->prev = src_most_next;
        src_most_next->next = next_dst;
    }
}

void add_instruction_bytecode_list(struct bytecode_list * dst, struct bytecode_list * instruction) {
    struct bytecode_list * next_to_dst = dst->next;
    if (next_to_dst != NULL) {
        next_to_dst->prev = instruction;
    }
    if (next_to_dst != NULL && next_to_dst->next != NULL) {
        instruction->next = next_to_dst;
    }

    dst->next = instruction;
    instruction->prev = dst;
}

struct bytecode_list * get_by_index_bytecode_list(struct bytecode_list * head_instruction, int target_index) {
    struct bytecode_list * current_bytecode = head_instruction;
    int current_index = 0;

    while(current_index != target_index){
        current_index += get_size_bytecode_instruction(current_bytecode->bytecode);
        current_bytecode = current_bytecode->next;
    }

    return current_bytecode;
}

void unlink_instruciton_bytecode_list(struct bytecode_list * instruction) {
    if(instruction->prev != NULL){
        instruction->prev->next = instruction->next;
    }
    if(instruction->next != NULL){
        instruction->next->prev = instruction->prev;
    }

    free(instruction);
}

struct bytecode_list * create_bytecode_list(struct chunk * chunk) {
    struct bytecode_list * head = malloc(sizeof(struct bytecode_list));
    struct bytecode_list * last_allocated = head;
    struct chunk_iterator chunk_iterator = iterate_chunk(chunk);

    while(has_next_chunk_iterator(&chunk_iterator)) {
        bytecode_t current_instruction = next_instruction_chunk_iterator(&chunk_iterator);

        struct bytecode_list * current_node = malloc(sizeof(struct bytecode_list));
        current_node->bytecode = current_instruction;
        last_allocated->next = current_node;
        current_node->prev = last_allocated;

        last_allocated = current_node;

        switch (current_instruction) {
            case OP_INITIALIZE_STRUCT:
            case OP_SET_STRUCT_FIELD:
            case OP_GET_STRUCT_FIELD:
            case OP_DEFINE_GLOBAL:
            case OP_ENTER_MONITOR:
            case OP_GET_GLOBAL:
            case OP_SET_GLOBAL:
            case OP_GET_LOCAL:
            case OP_SET_LOCAL:
            case OP_FAST_CONST_8:
            case OP_EXIT_MONITOR:
            case OP_CONSTANT:
            case OP_PACKAGE_CONST:
                current_node->as.u8 = read_u8_chunk_iterator(&chunk_iterator);
                break;
            case OP_CALL:
                current_node->as.pair.u8_1 = read_u8_chunk_iterator_at(&chunk_iterator, 0);
                current_node->as.pair.u8_2 = read_u8_chunk_iterator_at(&chunk_iterator, 1);

            case OP_JUMP_IF_FALSE:
            case OP_INITIALIZE_ARRAY:
            case OP_GET_ARRAY_ELEMENT:
            case OP_SET_ARRAY_ELEMENT:
            case OP_FAST_CONST_16:
            case OP_LOOP:
                current_node->as.u16 = read_u16_chunk_iterator(&chunk_iterator);
            default:
        }
    }
    
    return head;
}

static struct bytecode_list * get_last_instruction(struct bytecode_list * instruction) {
    while(instruction->next != NULL){
        instruction = instruction->next;
    }

    return instruction;
}