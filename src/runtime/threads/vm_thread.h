#pragma once

#include "shared/string_pool.h"
#include "shared/utils/collections//lox_hash_table.h"
#include "compiler/chunk/chunk.h"
#include "shared/types/function_object.h"
#include "shared.h"
#include "compiler/chunk/chunk_disassembler.h"
#include "compiler/compiler.h"
#include "shared/types/native_object.h"
#include "runtime/native/native_functions.h"
#include "runtime/memory/gc.h"
#include "shared/types/struct_instance_object.h"
#include "shared/utils/collections/stack_list.h"
#include "runtime/memory/gc.h"

#define STACK_MAX 256
#define FRAME_MAX (STACK_MAX * 256)
#define MAX_THREADS_PER_THREAD 64

struct call_frame {
    struct function_object * function;
    uint8_t * pc; //Actual instruction
    lox_value_t * slots; //Used for local variables. It points to the gray_stack

    int last_monitor_entered_index;
    int monitors_entered[MAX_MONITORS_PER_FUNCTION];
};

typedef enum {
    THREAD_NEW,
    THREAD_RUNNABLE,
    THREAD_WAITING,
    THREAD_TERMINATED
} vm_thread_state_t;

typedef enum {
    THREAD_TERMINATED_NONE,
    THREAD_TERMINATED_PENDING_GC,
    THREAD_TERMINATED_GC_DONE,
} vm_thread_terminated_state_t;

struct vm_thread {
    lox_thread_id thread_id;

    volatile vm_thread_state_t state;
    volatile vm_thread_terminated_state_t terminated_state;

    pthread_t native_thread;

    struct vm_thread * children[MAX_THREADS_PER_THREAD];
    struct vm_thread * parent;

    lox_value_t stack[STACK_MAX];
    lox_value_t * esp; //Top of stack_list
    struct call_frame frames[FRAME_MAX];
    int frames_in_use;

    struct package * current_package;
    struct stack_list package_stack;

    struct gc_thread_info * gc_info;
};

typedef void (*thread_consumer_t)(struct vm_thread * parent, struct vm_thread * child, int index, void * extra);

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

void for_each_thread(struct vm_thread * start_thread, thread_consumer_t callback, void * extra, long options);
