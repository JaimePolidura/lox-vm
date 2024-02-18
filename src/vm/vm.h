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
#include "vm/threads/vm_thread_id_pool.h"

struct vm {
    struct vm_thread * root;
    struct gc gc;

    struct thread_id_pool thread_id_pool;

    //It includes all states threads except TERMINATED
    volatile int number_current_threads;
    volatile int number_waiting_threads;

    //We need this mutex to solve the race condition if a gc is going to get started and a other thread calls a native call
    //The only way a thread can block is by calling a native function
    struct rw_mutex blocking_call_mutex;

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

void set_self_thread_runnable();
void set_self_thread_waiting();

void start_vm();
void stop_vm();