#pragma once

#include "shared/types/function_object.h"
#include "shared.h"

struct cpu_regs_state {
    uint64_t rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11, rsp, rbx, rbp, r12, r13, r14, r15;
};

void try_jit_compile(struct function_object * function);

void run_jit_compiled(struct function_object * function);