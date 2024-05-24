#pragma once

#include "runtime/native_functions/native_function_definer.h"
#include "runtime/threads/vm_thread.h"
#include "runtime/memory/gc_result.h"
#include "runtime/jit/jit_compiler.h"

#include "compiler/bytecode/chunk/chunk_iterator.h"
#include "compiler/bytecode/bytecode_compiler.h"

#include "shared/utils/collections/lox_hash_table.h"
#include "shared/types/native_function_object.h"
#include "shared/types/struct_instance_object.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/string_pool.h"

#include "shared.h"

struct vm {
    struct vm_thread * root;
    void * gc;

    lox_thread_id last_thread_id;

    //It includes all states threads_race_conditions except TERMINATED
    volatile int number_current_threads;
    volatile int number_waiting_threads;

#ifdef VM_TEST
    struct gc_result last_gc_result;

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