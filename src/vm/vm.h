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
#include "memory/gc/gc_global_info.h"
#include "types/struct_instance_object.h"
#include "utils/stack_list.h"
#include "vm/threads/vm_thread.h"
#include "vm/threads/vm_thread_id_pool.h"

struct vm {
    struct vm_thread * root;
    struct gc_global_info gc;

    struct thread_id_pool thread_id_pool;

    //It includes all states threads except TERMINATED
    //TODO Pending to update
    volatile int number_current_threads;

    volatile int number_threads_signaled_gc;

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