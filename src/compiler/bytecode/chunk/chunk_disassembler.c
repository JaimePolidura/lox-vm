#include "chunk_disassembler.h"

#include <stdio.h>

#define READ_BYTECODE(pc) (*pc++)
#define READ_U16(pc) (pc += 2, (uint16_t)((pc[-2] << 8) | pc[-1]))

#define SINGLE_INSTRUCTION(name) printf("%s\n", name)
#define BINARY_U8_INSTRUCTION(name, iterator) printf("%s %u\n", name, read_u8_chunk_iterator(&iterator))
#define BINARY_U16_INSTRUCTION(name, ite) printf("%s %u\n", name, read_u8_chunk_iterator(&ite))
#define BINARY_U64_INSTRUCTION(name, ite) printf("%s %llu\n", name, read_u64_chunk_iterator(&ite))
#define FWD_JUMP_INSTRUCTION(name, pc, chunk, jump) printf("%s %4llX\n", name, (pc + jump) - chunk->code)
#define BWD_JUMP_INSTRUCTION(name, pc, chunk, jump) printf("%s %4llX\n", name, (pc - jump) - chunk->code)

#define BINARY_STRING_INSTRUCTION(name, chunk, ite) printf("%s %s\n", name, AS_STRING_OBJECT(read_constant_chunk_iterator(&ite))->chars)
#define CALL_INSTRUCTION(name, ite) printf("%s %u %d\n", name, \
    read_u8_chunk_iterator_at(&ite, 0), read_u8_chunk_iterator_at(&ite, 1))
#define INITIALIZE_STRUCT_INSTRUCTION(namexd, ite) printf("%s <struct_definition: %s>\n", \
    namexd, ((struct struct_definition_object *) AS_OBJECT(read_constant_chunk_iterator(&ite)))->name->chars)
#define STRUCT_INSTRUCTION(name, pc, ite) printf("%s <field_name: %s>\n", name, AS_STRING_OBJECT(read_constant_chunk_iterator(&ite))->chars)
#define PACKAGE_CONST_INSTRUCTION(namexd, ite) printf("%s <package_name: %s>\n", namexd, ((struct package_object *) \
    AS_OBJECT(read_constant_chunk_iterator(&ite)))->package->name)

static void disassemble_function(struct function_object * function, long options);
static void disassemble_package_functions(struct package * package, long options);

void disassemble_package(struct package * package, long options) {
    printf("<main>:\n");
    disassemble_function(package->main_function, options);

    if((options & DISASSEMBLE_PACKAGE_FUNCTIONS) == DISASSEMBLE_PACKAGE_FUNCTIONS) {
        disassemble_package_functions(package, options);
    }
}

static void disassemble_package_functions(struct package * package, long options) {
    struct lox_arraylist package_constants = package->main_function->chunk->constants;

    for(int i = 0; i < package_constants.in_use; i++){
        lox_value_t current_constant = package_constants.values[i];

        if(IS_OBJECT(current_constant) && AS_OBJECT(current_constant)->type == OBJ_FUNCTION) {
            struct function_object * function = (struct function_object *) AS_OBJECT(current_constant);
            printf("\n<%s>:\n", function->name->chars);
            disassemble_function(function, options);
        }
    }
}

void disassemble_function(struct function_object * function, long options) {
    struct chunk_iterator iterator = iterate_chunk(function->chunk);

    while(has_next_chunk_iterator(&iterator)){
        printf("%4llX:\t", iterator.pc - function->chunk->code);

        switch (next_instruction_chunk_iterator(&iterator)) {
            case OP_RETURN: SINGLE_INSTRUCTION("OP_RETURN"); break;
            case OP_CONSTANT: BINARY_U8_INSTRUCTION("OP_CONSTANT", iterator); break;
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
            case OP_DEFINE_GLOBAL: BINARY_STRING_INSTRUCTION("OP_DEFINE_GLOBAL", chunk, iterator); break;
            case OP_GET_GLOBAL: BINARY_STRING_INSTRUCTION("OP_GET_GLOBAL", chunk, iterator); break;
            case OP_SET_GLOBAL: BINARY_STRING_INSTRUCTION("OP_SET_GLOBAL", chunk, iterator); break;
            case OP_GET_LOCAL: BINARY_U8_INSTRUCTION("OP_GET_LOCAL", iterator); break;
            case OP_JUMP_IF_FALSE: {
                uint16_t jump = read_u16_chunk_iterator(&iterator);
                FWD_JUMP_INSTRUCTION("OP_JUMP_IF_FALSE", iterator.pc, iterator.iterating, jump);
                break;
            }
            case OP_JUMP: {
                uint16_t jump = read_u16_chunk_iterator(&iterator);
                BWD_JUMP_INSTRUCTION("OP_JUMP", iterator.pc, iterator.iterating, jump);
                break;
            }
            case OP_SET_LOCAL: BINARY_U8_INSTRUCTION("OP_SET_LOCAL", iterator); break;
            case OP_LOOP: {
                uint16_t jump = read_u16_chunk_iterator(&iterator);
                BWD_JUMP_INSTRUCTION("OP_LOOP", iterator.pc, iterator.iterating, jump);
                break;
            }
            case OP_CALL: CALL_INSTRUCTION("OP_CALL", iterator); break;
            case OP_INITIALIZE_STRUCT: INITIALIZE_STRUCT_INSTRUCTION("OP_INITIALIZE_STRUCT", iterator); break;
            case OP_GET_STRUCT_FIELD: STRUCT_INSTRUCTION("OP_GET_STRUCT_FIELD", pc, iterator); break;
            case OP_SET_STRUCT_FIELD: STRUCT_INSTRUCTION("OP_SET_STRUCT_FIELD", pc, iterator); break;
            case OP_ENTER_PACKAGE: /*Ignored, printed by OP_PACKAGE_CONST*/ break;
            case OP_EXIT_PACKAGE: SINGLE_INSTRUCTION("OP_EXIT_PACKAGE"); break;
            case OP_ENTER_MONITOR: BINARY_U8_INSTRUCTION("OP_ENTER_MONITOR", iterator); break;
            case OP_EXIT_MONITOR: BINARY_U8_INSTRUCTION("OP_EXIT_MONITOR", iterator); break;
            case OP_INITIALIZE_ARRAY: BINARY_U16_INSTRUCTION("OP_INITIALIZE_ARRAY", iterator); break;
            case OP_GET_ARRAY_ELEMENT: BINARY_U16_INSTRUCTION("OP_GET_ARRAY_ELEMENT", iterator); break;
            case OP_SET_ARRAY_ELEMENT: BINARY_U16_INSTRUCTION("OP_SET_ARRAY_ELEMENT", iterator); break;
            case OP_FAST_CONST_8: BINARY_U8_INSTRUCTION("OP_FAST_CONST_8", iterator); break;
            case OP_FAST_CONST_16: BINARY_U16_INSTRUCTION("OP_FAST_CONST_8", iterator); break;
            case OP_CONST_1: SINGLE_INSTRUCTION("OP_CONST_1"); break;
            case OP_CONST_2: SINGLE_INSTRUCTION("OP_CONST_2"); break;
            case OP_PACKAGE_CONST: PACKAGE_CONST_INSTRUCTION("OP_ENTER_PACKAGE", iterator); break;
            case OP_EOF: SINGLE_INSTRUCTION("OP_EOF"); return;
            case OP_NO_OP: SINGLE_INSTRUCTION("OP_NO_OP"); break;
            case OP_ENTER_MONITOR_EXPLICIT: BINARY_U64_INSTRUCTION("OP_ENTER_MONITOR_EXPLICIT", iterator); break;
            case OP_EXIT_MONITOR_EXPLICIT: BINARY_U64_INSTRUCTION("OP_EXIT_MONITOR_EXPLICIT", iterator); break;
            default:
                perror("Unhandled bytecode op\n");
                exit(-1);
        }
    }
}