#include "chunk_instruction_list.h"

void free_chunk_instruction_list(struct chunk_instruction_list * chunk_instruction_list) {
    struct chunk_instruction_list * current_node = chunk_instruction_list;
    while(current_node != NULL) {
        struct chunk_instruction_list * next = current_node->next;
        free(current_node);
        current_node = next;
    }
}

struct chunk * to_chunk_instruction_list(struct chunk_instruction_list * chunk_instruction_list) {
    struct chunk * new_chunk = alloc_chunk();

    struct chunk_instruction_list * current_node = chunk_instruction_list;
    while(current_node != NULL) {
        int instruction_length = instruction_bytecode_length(current_node->bytecode);

        write_chunk(new_chunk, current_node->bytecode);

        if (instruction_length == 2) {
            write_chunk(new_chunk, current_node->as.u8);
        } else if(instruction_length == 3) {
            write_chunk(new_chunk, current_node->as.pair.u8_1);
            write_chunk(new_chunk, current_node->as.pair.u8_2);
        }

        current_node = current_node->next;
    }

    return new_chunk;
}

struct chunk_instruction_list * create_chunk_instruction_list(struct chunk * chunk) {
    struct chunk_instruction_list * head = malloc(sizeof(struct chunk_instruction_list));
    struct chunk_instruction_list * last_allocated = head;
    struct chunk_iterator chunk_iterator = iterate_chunk(chunk);

    while(has_next_chunk_iterator(&chunk_iterator)) {
        bytecode_t current_instruction = next_instruction_chunk_iterator(&chunk_iterator);

        struct chunk_instruction_list * current_node = malloc(sizeof(struct chunk_instruction_list));
        current_node->bytecode = current_instruction;
        last_allocated->next = current_node;
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