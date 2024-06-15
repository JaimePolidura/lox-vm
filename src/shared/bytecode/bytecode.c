#include "bytecode.h"

struct bytecode_instruction_data {
    int size;
    int n_pops;
    int n_push;
};

struct bytecode_instruction_data bytecode_instructions_data[] = {
        [OP_RETURN] = {.size = OP_RETURN_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_ADD] = {.size = OP_ADD_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_SUB] = {.size = OP_SUB_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_MUL] = {.size = OP_MUL_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_DIV] = {.size = OP_DIV_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_GREATER] = {.size = OP_GREATER_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_LESS] = {.size = OP_LESS_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_FALSE] =  {.size = OP_FALSE_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_TRUE] = {.size = OP_TRUE_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_NIL] = {.size = OP_NIL_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_NOT] = {.size = OP_NOT_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_EQUAL] = {.size = OP_EQUAL_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_PRINT] = {.size = OP_PRINT_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_NEGATE] = {.size = OP_NEGATE_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_POP] = {.size = OP_POP_LENGTH, .n_pops = 1, .n_push = 0},
        [OP_CONST_1] = {.size = OP_CONST_1_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_CONST_2] = {.size = OP_CONST_2_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_EOF] = {.size = OP_EOF_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_NO_OP] = {.size = OP_NO_OP_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_ENTER_PACKAGE] = {.size = OP_ENTER_PACKAGE_LENGTH, .n_pops = 1, .n_push = 0},
        [OP_EXIT_PACKAGE] = {.size = OP_EXIT_PACKAGE_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_INITIALIZE_STRUCT] = {.size = OP_INITIALIZE_STRUCT_LENGTH, .n_pops = N_VARIABLE_INSTRUCTION_N_POPS, .n_push = 1},
        [OP_SET_STRUCT_FIELD] = {.size = OP_SET_STRUCT_FIELD_LENGTH, .n_pops = 2, .n_push = 0},
        [OP_GET_STRUCT_FIELD] = {.size = OP_GET_STRUCT_FIELD_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_DEFINE_GLOBAL] = {.size = OP_DEFINE_GLOBAL_LENGTH, .n_pops = 1, .n_push = 0},
        [OP_ENTER_MONITOR] = {.size = OP_ENTER_MONITOR_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_GET_GLOBAL] = {.size = OP_GET_GLOBAL_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_SET_GLOBAL] = {.size = OP_SET_GLOBAL_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_GET_LOCAL] = {.size = OP_GET_LOCAL_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_SET_LOCAL] = {.size = OP_SET_LOCAL_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_FAST_CONST_8] = {.size = OP_FAST_CONST_8_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_EXIT_MONITOR] = {.size = OP_EXIT_MONITOR_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_CONSTANT] =  {.size = OP_CONSTANT_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_PACKAGE_CONST] = {.size = OP_PACKAGE_CONST_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_JUMP_IF_FALSE] = {.size = OP_JUMP_IF_FALSE_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_CALL] = {.size = OP_CALL_LENGTH, .n_pops = N_VARIABLE_INSTRUCTION_N_POPS, .n_push = 1}, //TODO
        [OP_INITIALIZE_ARRAY] = {.size = OP_INITIALIZE_ARRAY_LENGTH, .n_pops = N_VARIABLE_INSTRUCTION_N_POPS, .n_push = 1},
        [OP_GET_ARRAY_ELEMENT] = {.size = OP_GET_ARRAY_ELEMENT_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_SET_ARRAY_ELEMENT] = {.size = OP_SET_ARRAY_ELEMENT_LENGTH, .n_pops = 2, .n_push = 0},
        [OP_FAST_CONST_16] = {.size = OP_FAST_CONST_16_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_LOOP] = {.size = OP_LOOP_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_JUMP] = {.size = OP_JUMP_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_ENTER_MONITOR_EXPLICIT] = {.size = OP_ENTER_MONITOR_EXPLICIT_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_EXIT_MONITOR_EXPLICIT] = {.size = OP_EXIT_MONITOR_EXPLICIT_LENGTH, .n_pops = 0, .n_push = 0},
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