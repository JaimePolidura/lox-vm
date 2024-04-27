#include "chunk_disassembler.h"

#include <stdio.h>

#define READ_BYTECODE(pc) (*pc++)
#define READ_CONSTANT(chunk, pc) (chunk->constants.values[READ_BYTECODE(pc)])
#define READ_U16(pc) (pc += 2, (uint16_t)((pc[-2] << 8) | pc[-1]))

#define SINGLE_INSTRUCTION(name) printf("%s\n", name)
#define BINARY_U8_INSTRUCTION(name, pc) printf("%s %u\n", name, READ_BYTECODE(pc))
#define BINARY_U16_INSTRUCTION(name, pc) printf("%s %u\n", name, READ_U16(pc))
#define FWD_JUMP_INSTRUCTION(name, pc, chunk, jump) printf("%s %4llX\n", name, (pc + jump) - chunk->code + 1)
#define BWD_JUMP_INSTRUCTION(name, pc, chunk, jump) printf("%s %4llX\n", name, (pc - jump) - chunk->code + 1)

#define BINARY_STRING_INSTRUCTION(name, chunk, pc) printf("%s %s\n", name, AS_STRING_OBJECT(READ_CONSTANT(chunk, pc))->chars)
#define CALL_INSTRUCTION(name, pc) printf("%s %u %d\n", name, READ_BYTECODE(pc), READ_BYTECODE(pc))
#define INITIALIZE_STRUCT_INSTRUCTION(namexd, chunk, pc) printf("%s <struct_definition: %s>\n", \
    namexd, ((struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(chunk, pc)))->name->chars)
#define STRUCT_INSTRUCTION(name, pc, chunk) printf("%s <field_name: %s>\n", name, AS_STRING_OBJECT(READ_CONSTANT(chunk, pc))->chars)
#define PACKAGE_CONST_INSTRUCTION(namexd, pc, chunk) printf("%s <package_name: %s>\n", namexd, ((struct package_object *) AS_OBJECT(READ_CONSTANT(chunk, pc)))->package->name)

void disassemble_chunk(struct chunk * chunk) {
    uint8_t * pc = chunk->code;

    for(;(pc - chunk->code < chunk->in_use);) {
        uint8_t current_instruction = READ_BYTECODE(pc);

        printf("%4llX:\t", pc - chunk->code);

        switch (current_instruction) {
            case OP_RETURN: SINGLE_INSTRUCTION("OP_RETURN"); break;
            case OP_CONSTANT: BINARY_U8_INSTRUCTION("OP_CONSTANT", pc); break;
            case OP_NEGATE: SINGLE_INSTRUCTION("OP_NEGATE"); break;
            case OP_ADD: SINGLE_INSTRUCTION("OP_ADD"); break;
            case OP_SUB: SINGLE_INSTRUCTION("OP_SUB"); break;
            case OP_MUL: SINGLE_INSTRUCTION("OP_MUL"); break;
            case OP_DIV: SINGLE_INSTRUCTION("OP_DIV"); break;
            case OP_GREATER: SINGLE_INSTRUCTION("OP_GREATER"); break;
            case OP_LESS: SINGLE_INSTRUCTION("OP_LESS"); break;
            case OP_FALSE: SINGLE_INSTRUCTION("OP_FALSE"); break;
            case OP_TRUE: SINGLE_INSTRUCTION("OP_TRUE"); break;
            case OP_NIL: SINGLE_INSTRUCTION("OP_NIL"); break;
            case OP_NOT: SINGLE_INSTRUCTION("OP_NOT"); break;
            case OP_EQUAL: SINGLE_INSTRUCTION("OP_EQUAL"); break;
            case OP_PRINT: SINGLE_INSTRUCTION("OP_PRINT"); break;
            case OP_POP: SINGLE_INSTRUCTION("OP_POP"); break;
            case OP_DEFINE_GLOBAL: BINARY_STRING_INSTRUCTION("OP_DEFINE_GLOBAL", chunk, pc); break;
            case OP_GET_GLOBAL: BINARY_STRING_INSTRUCTION("OP_GET_GLOBAL", chunk, pc); break;
            case OP_SET_GLOBAL: BINARY_STRING_INSTRUCTION("OP_SET_GLOBAL", chunk, pc); break;
            case OP_GET_LOCAL: BINARY_U8_INSTRUCTION("OP_GET_LOCAL", pc); break;
            case OP_JUMP_IF_FALSE: { uint16_t jump = READ_U16(pc); FWD_JUMP_INSTRUCTION("OP_JUMP_IF_FALSE", pc, chunk, jump); break; }
            case OP_JUMP: { uint16_t jump = READ_U16(pc); BWD_JUMP_INSTRUCTION("OP_JUMP", pc, chunk, jump); break; }
            case OP_SET_LOCAL: BINARY_U8_INSTRUCTION("OP_SET_LOCAL", pc); break;
            case OP_LOOP: { uint16_t jump = READ_U16(pc); BWD_JUMP_INSTRUCTION("OP_LOOP", pc, chunk, jump); break; }
            case OP_CALL: CALL_INSTRUCTION("OP_CALL", pc); break;
            case OP_INITIALIZE_STRUCT: INITIALIZE_STRUCT_INSTRUCTION("OP_INITIALIZE_STRUCT", chunk, pc); break;
            case OP_GET_STRUCT_FIELD: STRUCT_INSTRUCTION("OP_GET_STRUCT_FIELD", pc, chunk); break;
            case OP_SET_STRUCT_FIELD: STRUCT_INSTRUCTION("OP_SET_STRUCT_FIELD", pc, chunk); break;
            case OP_ENTER_PACKAGE: /*Ignored, printed by OP_PACKAGE_CONST*/ break;
            case OP_EXIT_PACKAGE: SINGLE_INSTRUCTION("OP_EXIT_PACKAGE"); break;
            case OP_ENTER_MONITOR: BINARY_U8_INSTRUCTION("OP_ENTER_MONITOR", pc); break;
            case OP_EXIT_MONITOR: SINGLE_INSTRUCTION("OP_EXIT_MONITOR"); break;
            case OP_INITIALIZE_ARRAY: BINARY_U16_INSTRUCTION("OP_INITIALIZE_ARRAY", pc); break;
            case OP_GET_ARRAY_ELEMENT: BINARY_U16_INSTRUCTION("OP_GET_ARRAY_ELEMENT", pc); break;
            case OP_SET_ARRAY_ELEMENT: BINARY_U16_INSTRUCTION("OP_SET_ARRAY_ELEMENT", pc); break;
            case OP_FAST_CONST_8: BINARY_U8_INSTRUCTION("OP_FAST_CONST_8", pc); break;
            case OP_FAST_CONST_16: BINARY_U16_INSTRUCTION("OP_FAST_CONST_8", pc); break;
            case OP_CONST_1: SINGLE_INSTRUCTION("OP_CONST_1"); break;
            case OP_CONST_2: SINGLE_INSTRUCTION("OP_CONST_2"); break;
            case OP_PACKAGE_CONST: PACKAGE_CONST_INSTRUCTION("OP_ENTER_PACKAGE", pc, chunk); break;
            case OP_EOF: SINGLE_INSTRUCTION("OP_EOF"); return;
            case OP_NO_OP: SINGLE_INSTRUCTION("OP_NO_OP"); break;
            default:
                perror("Unhandled bytecode op\n");
        }
    }
}