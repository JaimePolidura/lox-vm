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
#include "memory/gc/gc_global_info.h"

#define STACK_MAX 256
#define FRAME_MAX (STACK_MAX * 256)
#define MAX_CHILD_THREADS_PER_THREAD 64

struct call_frame {
    struct function_object * function;
    uint8_t * pc; //Actual instruction
    lox_value_t * slots; //Used for local variables. It points to the gray_stack
};

typedef enum {
    THREAD_NEW,
    THREAD_RUNNABLE,
    THREAD_WAITING,
    THREAD_TERMINATED
} vm_thread_tate_t;

struct vm_thread {
    volatile vm_thread_tate_t state;

    lox_thread_id thread_id;

    pthread_t native_thread;

    lox_value_t stack[STACK_MAX];
    lox_value_t * esp; //Top of stack_list

    struct package * current_package;
    struct stack_list package_stack;

    struct call_frame frames[FRAME_MAX];
    int frames_in_use;

    struct vm_thread * children[MAX_CHILD_THREADS_PER_THREAD];

    struct gc_thread_info gc_info;

    struct mutex gc_signal_mutex;
    bool start_gc_pending_signal; //Written only by vm.c signal_threads_start_gc
    int last_gc_gen_signaled;
};

struct vm_thread * alloc_vm_thread();
void free_vm_thread(struct vm_thread * vm_thread);

void for_each_thread(struct vm_thread * start_thread, consumer_t callback);