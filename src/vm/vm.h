#pragma once

#include "memory/string_pool.h"
#include "table/table.h"
#include "chunk/chunk.h"
#include "types/function.h"
#include "shared.h"
#include "chunk/chunk_disassembler.h"
#include "compiler/compiler.h"
#include "types/native.h"
#include "native_functions.h"

#define STACK_MAX 256
#define FRAME_MAX (STACK_MAX * 256)

struct call_frame {
    struct function_object * function;
    uint8_t * pc; //Actual instruction
    lox_value_t * slots; //Used for local variables. It points to the stack
};

struct vm {
    struct call_frame frames[FRAME_MAX];
    int frames_in_use;
    lox_value_t stack[STACK_MAX];
    lox_value_t * esp; // Top of the stack
    struct object * heap; // Linked list of heap allocated objects
    struct hash_table global_variables;
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} interpret_result;

interpret_result interpret_vm(struct compilation_result compilation_result);
void define_native(char * function_name, native_fn native_function);


//TODO Should these functions be in the header file? Maybe move to .c as static
struct string_object * add_string(char * ptr, int length);

void push_stack_vm(lox_value_t value);
lox_value_t pop_stack_vm();

void start_vm();
void stop_vm();