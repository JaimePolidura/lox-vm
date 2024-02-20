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
#include "memory/gc/gc.h"

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
};

typedef void (*thread_consumer_t)(struct vm_thread * parent, struct vm_thread * child, int index);

struct vm_thread * alloc_vm_thread();
void free_vm_thread(struct vm_thread * vm_thread);

enum {
    THREADS_OPT_NONE = 0,
    //Useless, enabled by default. This might clarity the intentions of the caller of for_each_thread()
    THREADS_OPT_RECURSIVE = 0,
    THREADS_OPT_INCLUSIVE = 0,

    THREADS_OPT_EXCLUSIVE = 1,
    THREADS_OPT_NOT_RECURSIVE = 1 << 2,
    THREADS_OPT_INCLUDE_TERMINATED = 1 << 4
};

void for_each_thread(struct vm_thread * start_thread, thread_consumer_t callback, long options);