#pragma once

#include "memory/string_pool.h"
#include "utils/table.h"
#include "chunk/chunk.h"
#include "types/function.h"
#include "shared.h"
#include "chunk/chunk_disassembler.h"
#include "compiler/compiler.h"
#include "types/native.h"
#include "vm/native/native_functions.h"
#include "memory/gc/gc.h"
#include "types/struct_instance_object.h"
#include "utils/stack_list.h"
#include "vm/threads/vm_thread.h"

struct vm {
    struct vm_thread * root;
    struct gc * gc;

    lox_thread_id last_thread_id;

    //It includes all states threads_race_conditions except TERMINATED
    volatile int number_current_threads;
    volatile int number_waiting_threads;

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
void define_native(char * function_name, native_fn native_function, bool is_blocking);

void set_self_thread_runnable();
void set_self_thread_waiting();

void start_vm();
void stop_vm();