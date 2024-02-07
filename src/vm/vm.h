#pragma once

#include "memory/string_pool.h"
#include "utils/table.h"
#include "chunk/chunk.h"
#include "types/function.h"
#include "shared.h"
#include "chunk/chunk_disassembler.h"
#include "compiler/compiler.h"
#include "types/native.h"
#include "native_functions.h"
#include "memory/gc/gc.h"
#include "types/struct_object.h"
#include "utils/stack_list.h"

#define STACK_MAX 256
#define FRAME_MAX (STACK_MAX * 256)

struct call_frame {
    struct function_object * function;
    uint8_t * pc; //Actual instruction
    lox_value_t * slots; //Used for local variables. It points to the gray_stack
};

struct vm {
    struct package * current_package;
    struct stack_list package_stack;

    struct call_frame frames[FRAME_MAX];
    int frames_in_use;
    lox_value_t stack[STACK_MAX];
    lox_value_t * esp; //Top of stack_list
    struct gc gc;

#ifdef VM_TEST
    char * log [256];
    int log_entries_in_use;
#endif
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} interpret_result_t;

interpret_result_t interpret_vm(struct compilation_result compilation_result);
void define_native(char * function_name, native_fn native_function);

void start_vm();
void stop_vm();