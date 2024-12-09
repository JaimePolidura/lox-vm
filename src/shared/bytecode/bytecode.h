#pragma once

#include "shared.h"

typedef enum {
    OP_CONSTANT,                 // Index: 0
    OP_NIL,                      // Index: 1
    OP_TRUE,                     // Index: 2
    OP_FALSE,                    // Index: 3
    OP_POP,                      // Index: 4
    OP_EQUAL,                    // Index: 5
    OP_GREATER,                  // Index: 6
    OP_LESS,                     // Index: 7
    OP_RETURN,                   // Index: 8
    OP_NEGATE,                   // Index: 9
    OP_PRINT,                    // Index: 10
    OP_EOF,                      // Index: 11
    OP_ADD,                      // Index: 12
    OP_SUB,                      // Index: 13
    OP_DEFINE_GLOBAL,            // Index: 14
    OP_GET_GLOBAL,               // Index: 15
    OP_SET_GLOBAL,               // Index: 16
    OP_GET_LOCAL,                // Index: 17
    OP_SET_LOCAL,                // Index: 18
    OP_MUL,                      // Index: 19
    OP_DIV,                      // Index: 20
    OP_NOT,                      // Index: 21
    OP_JUMP_IF_FALSE,            // Index: 22
    OP_JUMP,                     // Index: 23
    OP_LOOP,                     // Index: 24
    OP_CALL,                     // Index: 25
    OP_INITIALIZE_STRUCT,        // Index: 26
    OP_GET_STRUCT_FIELD,         // Index: 27
    OP_SET_STRUCT_FIELD,         // Index: 28 WB
    OP_ENTER_PACKAGE,            // Index: 29
    OP_EXIT_PACKAGE,             // Index: 30
    OP_ENTER_MONITOR,            // Index: 31
    OP_EXIT_MONITOR,             // Index: 32
    OP_INITIALIZE_ARRAY,         // Index: 33
    OP_GET_ARRAY_ELEMENT,        // Index: 34
    OP_SET_ARRAY_ELEMENT,        // Index: 35 WB
    OP_FAST_CONST_8,             // Index: 36
    OP_FAST_CONST_16,            // Index: 37
    OP_CONST_1,                  // Index: 38
    OP_CONST_2,                  // Index: 38
    OP_NO_OP,                    // Index: 39
    OP_PACKAGE_CONST,            // Index: 40
    OP_ENTER_MONITOR_EXPLICIT,   // Index: 41
    OP_EXIT_MONITOR_EXPLICIT,    // Index: 42
    OP_BINARY_OP_AND,            // Index: 43
    OP_BINARY_OP_OR,             // Index: 44
    OP_LEFT_SHIFT,               // Index: 45
    OP_RIGHT_SHIFT,              // Index: 46
    OP_MODULO,                   // Index: 47
} bytecode_t;

#define OP_CONSTANT_LENGTH 2
#define OP_NIL_LENGTH 1
#define OP_TRUE_LENGTH 1
#define OP_FALSE_LENGTH 1
#define OP_POP_LENGTH 1
#define OP_EQUAL_LENGTH 1
#define OP_GREATER_LENGTH 1
#define OP_LESS_LENGTH 1
#define OP_RETURN_LENGTH 1
#define OP_NEGATE_LENGTH 1
#define OP_PRINT_LENGTH 1
#define OP_ADD_LENGTH 1
#define OP_SUB_LENGTH 1
#define OP_DEFINE_GLOBAL_LENGTH 2
#define OP_GET_GLOBAL_LENGTH 2
#define OP_SET_GLOBAL_LENGTH 2
#define OP_GET_LOCAL_LENGTH 2
#define OP_SET_LOCAL_LENGTH 2
#define OP_MUL_LENGTH 1
#define OP_DIV_LENGTH 1
#define OP_NOT_LENGTH 1
#define OP_JUMP_IF_FALSE_LENGTH 3
#define OP_JUMP_LENGTH 3
#define OP_LOOP_LENGTH 3
#define OP_CALL_LENGTH 3
#define OP_INITIALIZE_STRUCT_LENGTH 2
#define OP_GET_STRUCT_FIELD_LENGTH 2
#define OP_SET_STRUCT_FIELD_LENGTH 2
#define OP_ENTER_PACKAGE_LENGTH 1
#define OP_EXIT_PACKAGE_LENGTH 1
#define OP_ENTER_MONITOR_LENGTH 2
#define OP_EXIT_MONITOR_LENGTH 2
#define OP_INITIALIZE_ARRAY_LENGTH 4
#define OP_GET_ARRAY_ELEMENT_LENGTH 3
#define OP_SET_ARRAY_ELEMENT_LENGTH 3
#define OP_FAST_CONST_8_LENGTH 2
#define OP_FAST_CONST_16_LENGTH 3
#define OP_CONST_1_LENGTH 1
#define OP_CONST_2_LENGTH 1
#define OP_PACKAGE_CONST_LENGTH 2
#define OP_NO_OP_LENGTH 1
#define OP_EOF_LENGTH 1
#define OP_ENTER_MONITOR_EXPLICIT_LENGTH 9
#define OP_EXIT_MONITOR_EXPLICIT_LENGTH 9
#define OP_BINARY_OP_AND_LENGTH 1
#define OP_BINARY_OP_OR_LENGTH 1
#define OP_LEFT_SHIFT_LENGTH 1
#define OP_RIGHT_SHIFT_LENGTH 1
#define OP_MODULO_LENGTH 1

#define N_VARIABLE_INSTRUCTION_N_POPS 0xFFFF

bool is_fwd_jump_bytecode_instruction(bytecode_t);
bool is_bwd_jump_bytecode_instruction(bytecode_t);
bool is_jump_bytecode_instruction(bytecode_t);
bool is_constant_bytecode_instruction(bytecode_t);
bool is_commutative_associative_bytecode_instruction(bytecode_t);

int get_n_push_bytecode_instruction(bytecode_t);
int get_n_pop_bytecode_instruction(bytecode_t);

int get_size_bytecode_instruction(bytecode_t);
