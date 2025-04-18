#include "bytecode.h"

struct bytecode_instruction_data {
    int size;
    int n_pops;
    int n_push;
    bool constant;
};

struct bytecode_instruction_data bytecode_instructions_data[] = {
        [OP_RETURN] = {.constant = false, .size = OP_RETURN_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_ADD] = {.constant = false, .size = OP_ADD_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_SUB] = {.constant = false, .size = OP_SUB_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_MUL] = {.constant = false, .size = OP_MUL_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_DIV] = {.constant = false, .size = OP_DIV_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_GREATER] = {.constant = false, .size = OP_GREATER_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_LESS] = {.constant = false, .size = OP_LESS_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_FALSE] = {.constant = true, .size = OP_FALSE_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_TRUE] = {.constant = true, .size = OP_TRUE_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_NIL] = {.constant = true, .size = OP_NIL_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_NOT] = {.constant = false, .size = OP_NOT_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_EQUAL] = {.constant = false, .size = OP_EQUAL_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_PRINT] = {.constant = false, .size = OP_PRINT_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_NEGATE] = {.constant = false, .size = OP_NEGATE_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_POP] = {.constant = false, .size = OP_POP_LENGTH, .n_pops = 1, .n_push = 0},
        [OP_CONST_1] = {.constant = true, .size = OP_CONST_1_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_CONST_2] = {.constant = true, .size = OP_CONST_2_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_EOF] = {.constant = false, .size = OP_EOF_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_NO_OP] = {.constant = false, .size = OP_NO_OP_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_ENTER_PACKAGE] = {.constant = false, .size = OP_ENTER_PACKAGE_LENGTH, .n_pops = 1, .n_push = 0},
        [OP_EXIT_PACKAGE] = {.constant = false, .size = OP_EXIT_PACKAGE_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_INITIALIZE_STRUCT] = {.constant = false, .size = OP_INITIALIZE_STRUCT_LENGTH, .n_pops = N_VARIABLE_INSTRUCTION_N_POPS, .n_push = 1},
        [OP_SET_STRUCT_FIELD] = {.constant = false, .size = OP_SET_STRUCT_FIELD_LENGTH, .n_pops = 2, .n_push = 0},
        [OP_GET_STRUCT_FIELD] = {.constant = false, .size = OP_GET_STRUCT_FIELD_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_DEFINE_GLOBAL] = {.constant = false, .size = OP_DEFINE_GLOBAL_LENGTH, .n_pops = 1, .n_push = 0},
        [OP_ENTER_MONITOR] = {.constant = false, .size = OP_ENTER_MONITOR_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_GET_GLOBAL] = {.constant = false, .size = OP_GET_GLOBAL_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_SET_GLOBAL] = {.constant = false, .size = OP_SET_GLOBAL_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_GET_LOCAL] = {.constant = false, .size = OP_GET_LOCAL_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_SET_LOCAL] = {.constant = false, .size = OP_SET_LOCAL_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_FAST_CONST_8] = {.constant = true, .size = OP_FAST_CONST_8_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_EXIT_MONITOR] = {.constant = false, .size = OP_EXIT_MONITOR_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_CONSTANT] =  {.constant = true, .size = OP_CONSTANT_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_PACKAGE_CONST] = {.constant = false, .size = OP_PACKAGE_CONST_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_JUMP_IF_FALSE] = {.constant = false, .size = OP_JUMP_IF_FALSE_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_CALL] = {.constant = false, .size = OP_CALL_LENGTH, .n_pops = N_VARIABLE_INSTRUCTION_N_POPS, .n_push = 1},
        [OP_INITIALIZE_ARRAY] = {.constant = false, .size = OP_INITIALIZE_ARRAY_LENGTH, .n_pops = N_VARIABLE_INSTRUCTION_N_POPS, .n_push = 1},
        [OP_GET_ARRAY_ELEMENT] = {.constant = false, .size = OP_GET_ARRAY_ELEMENT_LENGTH, .n_pops = 1, .n_push = 1},
        [OP_SET_ARRAY_ELEMENT] = {.constant = false, .size = OP_SET_ARRAY_ELEMENT_LENGTH, .n_pops = 2, .n_push = 0},
        [OP_FAST_CONST_16] = {.constant = true, .size = OP_FAST_CONST_16_LENGTH, .n_pops = 0, .n_push = 1},
        [OP_LOOP] = {.constant = false, .size = OP_LOOP_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_JUMP] = {.constant = false, .size = OP_JUMP_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_ENTER_MONITOR_EXPLICIT] = {.constant = false, .size = OP_ENTER_MONITOR_EXPLICIT_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_EXIT_MONITOR_EXPLICIT] = {.constant = false, .size = OP_EXIT_MONITOR_EXPLICIT_LENGTH, .n_pops = 0, .n_push = 0},
        [OP_RIGHT_SHIFT] = {.constant = false, .size = OP_RIGHT_SHIFT_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_LEFT_SHIFT] = {.constant = false, .size = OP_LEFT_SHIFT_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_BINARY_OP_AND] = {.constant = false, .size = OP_BINARY_OP_AND_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_BINARY_OP_OR] = {.constant = false, .size = OP_BINARY_OP_OR_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_MODULO] = {.constant = false, .size = OP_MODULO_LENGTH, .n_pops = 2, .n_push = 1},
        [OP_GET_ARRAY_LENGTH] = {.constant = false, .size = OP_GET_ARRAY_LENGTH_LENGTH, .n_pops = 1, .n_push = 1},
 };

bool is_commutative_associative_bytecode_instruction(bytecode_t instruction) {
    return instruction == OP_ADD || instruction == OP_MUL || instruction == OP_EQUAL || instruction == OP_BINARY_OP_AND || instruction == OP_BINARY_OP_OR;
}

int get_size_bytecode_instruction(bytecode_t instruction) {
    return bytecode_instructions_data[instruction].size;
}

int get_n_pop_bytecode_instruction(bytecode_t instruction) {
    return bytecode_instructions_data[instruction].n_pops;
}

int get_n_push_bytecode_instruction(bytecode_t instruction) {
    return bytecode_instructions_data[instruction].n_push;
}

bool is_constant_bytecode_instruction(bytecode_t instruction) {
    return bytecode_instructions_data[instruction].constant;
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

bool is_comparation_bytecode_instruction(bytecode_t instruction) {
    return instruction == OP_EQUAL || instruction == OP_GREATER || instruction == OP_LESS;
}