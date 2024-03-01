#pragma once

#include "shared.h"

typedef enum {
    OP_CONSTANT,          // 0
    OP_NIL,               // 1
    OP_TRUE,              // 2
    OP_FALSE,             // 3
    OP_POP,               // 4
    OP_EQUAL,             // 5
    OP_GREATER,           // 6
    OP_LESS,              // 7
    OP_RETURN,            // 8
    OP_NEGATE,            // 9
    OP_PRINT,             // 10
    OP_EOF,               // 11
    OP_ADD,               // 12
    OP_SUB,               // 13
    OP_DEFINE_GLOBAL,     // 14
    OP_GET_GLOBAL,        // 15
    OP_SET_GLOBAL,        // 16
    OP_GET_LOCAL,         // 17
    OP_SET_LOCAL,         // 18
    OP_MUL,               // 19
    OP_DIV,               // 20
    OP_NOT,               // 21
    OP_JUMP_IF_FALSE,     // 22
    OP_JUMP,              // 23
    OP_LOOP,              // 24
    OP_CALL,              // 25

    OP_INITIALIZE_STRUCT, // 26
    OP_GET_STRUCT_FIELD,  // 27
    OP_SET_STRUCT_FIELD,  // 28
    OP_ENTER_PACKAGE,     // 29
    OP_EXIT_PACKAGE,      // 30
    OP_ENTER_MONITOR,     // 31
    OP_EXIT_MONITOR,      // 32
    OP_INITIALIZE_ARRAY,  // 33
    OP_GET_ARRAY_ELEMENT, // 34
    OP_SET_ARRAY_ELEMENT, // 35

} op_code;

