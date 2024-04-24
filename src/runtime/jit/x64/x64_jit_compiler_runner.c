#include "shared/types/function_object.h"
#include "runtime/threads/vm_thread.h"

extern __thread struct vm_thread * self_thread;
extern bool restore_prev_call_frame();

static inline __attribute__((always_inline)) void load_self_thread_register();
static inline __attribute__((always_inline)) void push_cpu_regs();
static inline __attribute__((always_inline)) void pop_cpu_regs();

void run_jit_compiled_arch(struct function_object * function_object) {
    push_cpu_regs(); //Save caller regs
    load_self_thread_register();

    uint64_t restore_prev_call_frame_addr = (uint64_t) &restore_prev_call_frame;
    asm(
            "movq    %0, %%r9\n" // Use leaq for loading address
            :
            : "m" (restore_prev_call_frame_addr) // Pass the address as an input operand
            );

    asm(
            "pushq   %rbp\n"
            "movq    %rsp, %rbp\n"
            "movq    %rsp, %rcx\n"
            "movq    %rbp, %rdx\n"
            "movq    0xa20(%rbx), %rsp\n"
            "movq    0xa20(%rbx), %rbp\n"
            "subq    $0x10, %rbp\n"
            "subq    $0x8, %rbp\n"
            "movq    0x8(%rbp), %r15\n"
            "movq    0x10(%rbp), %r14\n"
            "addq    %r14, %r15\n"
            "addq    $0x8, %rsp\n"
            "movq    %r15, 0x18(%rbp)\n"
            "movq    0x18(%rbp), %r14\n"
            "movq    %rbx, %r13\n"
            "movq    0xa28(%r13), %r13\n"
            "movq    %rsp, (%r13)\n"
            "movq    %rbp, 0x8(%r13)\n"
            "movq    %rbx, 0x10(%r13)\n"
            "movq    %rcx, %rsp\n"
            "movq    %rdx, %rbp\n"
            "pushq   %r13\n"
            "pushq   %r9\n"
            "callq   *%r9\n"
            "popq    %r9\n"
            "popq    %r13\n"
            "movq    %rsp, %rcx\n"
            "movq    %rbp, %rbx\n"
            "movq    (%r13), %rsp\n"
            "movq    0x8(%r13), %rbp\n"
            "movq    0x10(%r13), %rbx\n"
            "movq    %rbp, %rsp\n"
            "movq    %r14, (%rsp)\n"
            "addq    $0x8, %rsp\n"
            "movq    %rsp, 0xa20(%rbx)\n"
            "movq    %rcx, %rsp\n"
            "movq    %rdx, %rbp\n"
            "popq    %rbp\n"
            "retq"
            );

    function_object->jit_info.compiled_jit();
    pop_cpu_regs(); //Resotre caller regs
}

static inline void load_self_thread_register() {
    __asm__(
            "movq %0, %%rbx"
            :
            : "r" (self_thread)
            : "%rbx"
            );
}

//We need to force the inline, otherwise it would result in a segfault.
//This happens because in order for the function to return to the caller,
//it saves the caller adddress in the stack at the beginning of the functino call,
//As we are pushing data in the stack, it would read the wrong address
static inline __attribute__((always_inline)) void push_cpu_regs() {
    __asm__(
            "pushq %%rax\n\t"
            "pushq %%rcx\n\t"
            "pushq %%rdx\n\t"
            "pushq %%rbx\n\t"
            "pushq %%rsi\n\t"
            "pushq %%rdi\n\t"
            "pushq %%r8\n\t"
            "pushq %%r9\n\t"
            "pushq %%r10\n\t"
            "pushq %%r11\n\t"
            "pushq %%r12\n\t"
            "pushq %%r13\n\t"
            "pushq %%r14\n\t"
            "pushq %%r15\n\t"
            :
            :
            :
            );
}

__attribute__((always_inline)) void pop_cpu_regs() {
    __asm__(
            "popq %%r15\n\t"
            "popq %%r14\n\t"
            "popq %%r13\n\t"
            "popq %%r12\n\t"
            "popq %%r11\n\t"
            "popq %%r10\n\t"
            "popq %%r9\n\t"
            "popq %%r8\n\t"
            "popq %%rdi\n\t"
            "popq %%rsi\n\t"
            "popq %%rbx\n\t"
            "popq %%rdx\n\t"
            "popq %%rcx\n\t"
            "popq %%rax\n\t"
            :
            :
            :
            );
}