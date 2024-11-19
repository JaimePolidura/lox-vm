#include "bytecode_list.h"

static struct bytecode_list * get_last_instruction(struct bytecode_list * instruction);
static void restore_jump_references(struct bytecode_list * referencee, struct bytecode_list * new_reference);
static void check_pending_jumps_to_resolve(struct pending_jumps_to_resolve * pending_jumps, int current_bytecode_index,
                                           struct bytecode_list * current_node);
static void calculate_to_chunk_index(struct bytecode_list * head);

struct bytecode_list * create_instruction_bytecode_list(bytecode_t bytecode) {
    struct bytecode_list * bytecode_list = alloc_bytecode_list();
    bytecode_list->bytecode = bytecode;
    return bytecode_list;
}

struct bytecode_list * alloc_bytecode_list() {
    struct bytecode_list * bytecode_list = malloc(sizeof(struct bytecode_list));
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
    calculate_to_chunk_index(bytecode_list);

    struct bytecode_list * current_instruction = bytecode_list;
    while(current_instruction != NULL) {
        int current_instruction_size = get_size_bytecode_instruction(current_instruction->bytecode);
        int current_index = new_chunk->in_use;

        write_chunk(new_chunk, current_instruction->bytecode);

        if (is_jump_bytecode_instruction(current_instruction->bytecode)) {
            int to_jump_index = current_instruction->as.jump->to_chunk_index;
            int jump_offset = abs(to_jump_index - current_index - 3);

            write_chunk(new_chunk, (jump_offset >> 8) & 0xff);
            write_chunk(new_chunk, jump_offset & 0xff);
        } else if (current_instruction_size == 2) {
            write_chunk(new_chunk, current_instruction->as.u8);
        } else if(current_instruction_size == 3) {
            write_chunk(new_chunk, current_instruction->as.pair.u8_1);
            write_chunk(new_chunk, current_instruction->as.pair.u8_2);
        } else if (current_instruction_size == 9) {
            write_chunk(new_chunk, (current_instruction->as.u64 >> 54) & 0xff);
            write_chunk(new_chunk, (current_instruction->as.u64 >> 48) & 0xff);
            write_chunk(new_chunk, (current_instruction->as.u64 >> 40) & 0xff);
            write_chunk(new_chunk, (current_instruction->as.u64 >> 32) & 0xff);
            write_chunk(new_chunk, (current_instruction->as.u64 >> 24) & 0xff);
            write_chunk(new_chunk, (current_instruction->as.u64 >> 16) & 0xff);
            write_chunk(new_chunk, (current_instruction->as.u64 >>  8) & 0xff);
            write_chunk(new_chunk, (current_instruction->as.u64 >>  0) & 0xff);
        } else if (current_instruction_size == 4 && current_instruction->bytecode == OP_INITIALIZE_ARRAY) {
            write_chunk(new_chunk, (current_instruction->as.initialize_array.n_elements >> 8) & 0xff);
            write_chunk(new_chunk, current_instruction->as.initialize_array.n_elements & 0xff);
            write_chunk(new_chunk, current_instruction->as.initialize_array.is_emtpy_initializaion & 0xff);
        }

        current_instruction = current_instruction->next;
    }

    return new_chunk;
}

static void calculate_to_chunk_index(struct bytecode_list * node) {
    int next_index = 0;

    while(node != NULL) {
        node->to_chunk_index = next_index;
        next_index += get_size_bytecode_instruction(node->bytecode);

        node = node->next;
    }
}

void add_first_instruction_bytecode_list(struct bytecode_list * head, struct bytecode_list * instruction) {
    struct bytecode_list * current = head;
    while (current->prev != NULL) {
        current = current->prev;
    }

    current->prev = instruction;
    instruction->next = current;
}

void add_last_instruction_bytecode_list(struct bytecode_list * head, struct bytecode_list * instruction) {
    struct bytecode_list * current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = instruction;
    instruction->prev = current;
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

void unlink_instruction_bytecode_list(struct bytecode_list * instruction) {
    if(instruction->prev != NULL){
        instruction->prev->next = instruction->next;
    }
    if(instruction->next != NULL){
        instruction->next->prev = instruction->prev;
    }

    restore_jump_references(instruction, instruction->next);

    free(instruction);
}

static void restore_jump_references(struct bytecode_list * referencee, struct bytecode_list * new_reference) {
    struct bytecode_list * current_node = referencee->prev;

    while(current_node != NULL) {
        if(current_node->as.jump == referencee){
            current_node->as.jump = new_reference;
        }

        current_node = current_node->prev;
    }
}

struct bytecode_list * create_bytecode_list(struct chunk * chunk) {
    struct pending_jumps_to_resolve pending_jumps;
    init_pending_jumps_to_resolve(&pending_jumps, chunk->in_use);
    struct bytecode_list * head = NULL;
    struct bytecode_list * last_allocated = NULL;
    struct chunk_iterator chunk_iterator = iterate_chunk(chunk);

    while(has_next_chunk_iterator(&chunk_iterator)) {
        bytecode_t current_instruction = next_instruction_chunk_iterator(&chunk_iterator);
        int current_instruction_index = current_instruction_index_chunk_iterator(&chunk_iterator);
        
        struct bytecode_list * current_node = alloc_bytecode_list();
        current_node->original_chunk_index = current_instruction_index;
        current_node->bytecode = current_instruction;
        current_node->prev = last_allocated;

        if(last_allocated != NULL)
            last_allocated->next = current_node;
        if(last_allocated == NULL)
            head = current_node;

        last_allocated = current_node;

        check_pending_jumps_to_resolve(&pending_jumps, current_instruction_index, current_node);

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
            case OP_PACKAGE_CONST: {
                current_node->as.u8 = read_u8_chunk_iterator(&chunk_iterator);
                break;
            }
            case OP_CALL: {
                current_node->as.pair.u8_1 = read_u8_chunk_iterator_at(&chunk_iterator, 0);
                current_node->as.pair.u8_2 = read_u8_chunk_iterator_at(&chunk_iterator, 1);
                break;
            }
            case OP_JUMP_IF_FALSE:
            case OP_JUMP: {
                int jmp_bytecode_offset = read_u16_chunk_iterator(&chunk_iterator);
                int to_jump_index = (current_instruction_index + 3) + jmp_bytecode_offset;
                add_pending_jump_to_resolve(&pending_jumps, to_jump_index, current_node);
                break;
            }
            case OP_LOOP: {
                int to_jump_instruction_index = current_instruction_index_chunk_iterator(&chunk_iterator) - read_u16_chunk_iterator(&chunk_iterator) + 3;
                struct bytecode_list * to_jump_bytecode = get_by_index_bytecode_list(head, to_jump_instruction_index);
                current_node->as.jump = to_jump_bytecode;
                to_jump_bytecode->loop_condition = true;
                break;
            }
            case OP_INITIALIZE_ARRAY: {
                current_node->as.initialize_array.n_elements = read_u16_chunk_iterator(&chunk_iterator);
                current_node->as.initialize_array.is_emtpy_initializaion = read_u8_chunk_iterator(&chunk_iterator);
                break;
            }
            case OP_GET_ARRAY_ELEMENT:
            case OP_SET_ARRAY_ELEMENT:
            case OP_FAST_CONST_16: {
                current_node->as.u16 = read_u16_chunk_iterator(&chunk_iterator);
                break;
            }
            case OP_ENTER_MONITOR_EXPLICIT:
            case OP_EXIT_MONITOR_EXPLICIT: {
                current_node->as.u64 = read_u64_chunk_iterator(&chunk_iterator);
                break;
            }
            default:
                exit(-1);
        }
    }

    free_pending_jumps_to_resolve(&pending_jumps);

    return head;
}

static void check_pending_jumps_to_resolve(struct pending_jumps_to_resolve * pending_jumps, int current_bytecode_index,
                                           struct bytecode_list * current_node) {
    struct pending_jump_to_resolve pending = get_pending_jump_to_resolve(pending_jumps, current_bytecode_index);

    for (int i = 0; i < pending.in_use; i++) {
        struct bytecode_list * other_node = pending.pending_resolve_data[i];
        other_node->as.jump = current_node;
    }
}

static struct bytecode_list * get_last_instruction(struct bytecode_list * instruction) {
    while(instruction->next != NULL){
        instruction = instruction->next;
    }

    return instruction;
}

struct bytecode_list * get_first_bytecode_list(struct bytecode_list * head) {
    struct bytecode_list * current = head;

    while (head->prev != NULL) {
        current = current->prev;
    }

    return current;
}