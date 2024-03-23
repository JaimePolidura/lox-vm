#pragma

#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/os/os_utils.h"
#include "compiler/bytecode.h"

struct cpu_regs {
    uint64_t rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11, rsp, rbx, rbp, r12, r13, r14, r15;
};

struct jit_compiler {
    struct function_object * function_to_compile;

    struct stack_list stack;

    uint8_t * pc;

    uint8_t * compiled_code;
};

jit_compiled jit_compile(struct function_object * function);

struct cpu_regs save_cpu_state();
void restore_cpu_state(struct cpu_regs * regs);