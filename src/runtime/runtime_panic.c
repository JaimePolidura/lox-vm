#include "runtime/threads/vm_thread.h"

extern __thread struct vm_thread * self_thread;

static inline struct call_frame * get_current_frame();
static void print_frame_stack_trace();

void runtime_panic(char * format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[Runtime] ");
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    struct call_frame * current_frame = get_current_frame();

    print_frame_stack_trace();

    exit(1);
}

static inline struct call_frame * get_current_frame() {
    return &self_thread->frames[self_thread->frames_in_use - 1];
}

static void print_frame_stack_trace() {
    for (int i = self_thread->frames_in_use - 1; i >= 0; i--) {
        struct call_frame * frame = &self_thread->frames[i];
        struct function_object * function = frame->function;

        size_t instruction = frame->pc - function->chunk->code - 1;
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
}