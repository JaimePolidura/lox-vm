#include "bytecode.h"

struct bytecode_instruction_data {
    int size;
    int n_pops;
    int n_push;
};

struct bytecode_instruction_data bytecode_instructions_data[] = {
        [OP_RETURN] = {.size = 1, .n_pops = 1, .n_push = 1},
        [OP_ADD] = {.size = 1, .n_pops = 2, .n_push = 1},
        [OP_SUB] = {.size = 1, .n_pops = 2, .n_push = 1},
        [OP_MUL] = {.size = 1, .n_pops = 2, .n_push = 1},
        [OP_DIV] = {.size = 1, .n_pops = 2, .n_push = 1},
        [OP_GREATER] = {.size = 1, .n_pops = 2, .n_push = 1},
        [OP_LESS] = {.size = 1, .n_pops = 2, .n_push = 1},
        [OP_FALSE] =  {.size = 1, .n_pops = 0, .n_push = 1},
        [OP_TRUE] = {.size = 1, .n_pops = 0, .n_push = 1},
        [OP_NIL] = {.size = 1, .n_pops = 0, .n_push = 1},
        [OP_NOT] = {.size = 1, .n_pops = 1, .n_push = 1},
        [OP_EQUAL] = {.size = 1, .n_pops = 1, .n_push = 1},
        [OP_PRINT] = {.size = 1, .n_pops = 2, .n_push = 1},
        [OP_NEGATE] = {.size = 1, .n_pops = 1, .n_push = 1},
        [OP_POP] = {.size = 1, .n_pops = 1, .n_push = 0},
        [OP_CONST_1] = {.size = 1, .n_pops = 0, .n_push = 1},
        [OP_CONST_2] = {.size = 1, .n_pops = 0, .n_push = 1},
        [OP_EOF] = {.size = 1, .n_pops = 0, .n_push = 0},
        [OP_NO_OP] = {.size = 1, .n_pops = 0, .n_push = 0},
        [OP_ENTER_PACKAGE] = {.size = 1, .n_pops = 1, .n_push = 0},
        [OP_EXIT_PACKAGE] = {.size = 1, .n_pops = 1, .n_push = 0},
        [OP_INITIALIZE_STRUCT] = {.size = 2, .n_pops = N_VARIABLE_INSTRUCTION_N_POPS, .n_push = 1},
        [OP_SET_STRUCT_FIELD] = {.size = 2, .n_pops = 2, .n_push = 0},
        [OP_GET_STRUCT_FIELD] = {.size = 2, .n_pops = 1, .n_push = 1},
        [OP_DEFINE_GLOBAL] = {.size = 2, .n_pops = 1, .n_push = 0},
        [OP_ENTER_MONITOR] = {.size = 2, .n_pops = 0, .n_push = 0},
        [OP_GET_GLOBAL] = {.size = 2, .n_pops = 0, .n_push = 1},
        [OP_SET_GLOBAL] = {.size = 2, .n_pops = 0, .n_push = 0},
        [OP_GET_LOCAL] = {.size = 2, .n_pops = 0, .n_push = 1},
        [OP_SET_LOCAL] = {.size = 2, .n_pops = 0, .n_push = 0},
        [OP_FAST_CONST_8] = {.size = 2, .n_pops = 0, .n_push = 1},
        [OP_EXIT_MONITOR] = {.size = 2, .n_pops = 0, .n_push = 0},
        [OP_CONSTANT] =  {.size = 2, .n_pops = 0, .n_push = 1},
        [OP_PACKAGE_CONST] = {.size = 2, .n_pops = 0, .n_push = 1},
        [OP_JUMP_IF_FALSE] = {.size = 3, .n_pops = 0, .n_push = 0},
        [OP_CALL] = {.size = 3, .n_pops = N_VARIABLE_INSTRUCTION_N_POPS, .n_push = 1}, //TODO
        [OP_INITIALIZE_ARRAY] = {.size = 3, .n_pops = -N_VARIABLE_INSTRUCTION_N_POPS, .n_push = 1},
        [OP_GET_ARRAY_ELEMENT] = {.size = 3, .n_pops = 1, .n_push = 1},
        [OP_SET_ARRAY_ELEMENT] = {.size = 3, .n_pops = 2, .n_push = 0},
        [OP_FAST_CONST_16] = {.size = 3, .n_pops = 0, .n_push = 1},
        [OP_LOOP] = {.size = 3, .n_pops = 0, .n_push = 0},
};

int get_size_bytecode_instruction(bytecode_t instruction) {
    return bytecode_instructions_data[instruction].size;
}

int get_n_pop_bytecode_instruction(bytecode_t instruction) {
    return bytecode_instructions_data[instruction].n_pops;
}

int get_n_push_bytecode_instruction(bytecode_t instruction) {
    return bytecode_instructions_data[instruction].n_push;
}

bool is_jump_bytecode_instruction(bytecode_t instruction) {
    return instruction == OP_JUMP_IF_FALSE || instruction == OP_JUMP || instruction == OP_LOOP;
}

bool is_fwd_jump_bytecode_instruction(bytecode_t instruction) {
    return instruction == OP_JUMP_IF_FALSE || instruction == OP_JUMP;
}

bool is_bwd_jump_bytecode_instruction(bytecode_t instruction) {
    return instruction == OP_LOOP;
}