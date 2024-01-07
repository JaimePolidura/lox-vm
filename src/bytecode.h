#pragma once

#include "shared.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_RETURN,
    OP_NEGATE,
    OP_PRINT,
    OP_EOF,
    OP_ADD,
    OP_SUB,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_MUL,
    OP_DIV,
    OP_NOT,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_LOOP,
    OP_CALL
} op_code;

