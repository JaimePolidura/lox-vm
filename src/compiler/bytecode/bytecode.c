#include "bytecode.h"

int instruction_bytecode_length(bytecode_t instruction) {
    switch (instruction) {
        case OP_RETURN:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_GREATER:
        case OP_LESS:
        case OP_FALSE:
        case OP_TRUE:
        case OP_NIL:
        case OP_NOT:
        case OP_EQUAL:
        case OP_PRINT:
        case OP_NEGATE:
        case OP_POP:
        case OP_CONST_1:
        case OP_CONST_2:
        case OP_EOF:
        case OP_NO_OP:
        case OP_ENTER_PACKAGE:
        case OP_EXIT_PACKAGE:
            return 1;
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
            return 2;
        case OP_JUMP_IF_FALSE:
        case OP_CALL:
        case OP_INITIALIZE_ARRAY:
        case OP_GET_ARRAY_ELEMENT:
        case OP_SET_ARRAY_ELEMENT:
        case OP_FAST_CONST_16:
        case OP_LOOP:
            return 3;
        default:
            return -1;
    }
}