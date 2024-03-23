#pragma

#include "shared/utils/collections/u8_arraylist.h"
#include "shared/types/function_object.h"
#include "shared/os/os_utils.h"
#include "compiler/bytecode.h"
#include "runtime/jit/x86/register_allocator.h"
#include "runtime/jit/x86/registers.h"
#include "runtime/jit/x86/opcodes.h"

struct cpu_regs_state {
    uint64_t regs[N_MAX_REGISTERS];
};

struct jit_compiler {
    struct function_object * function_to_compile;

    //Next bytecode instruction that we are compiling
    uint8_t * pc;

    struct register_allocator register_allocator;

    struct u8_arraylist compiled_code;
};

jit_compiled jit_compile(struct function_object * function);

struct cpu_regs_state save_cpu_state();
void restore_cpu_state(struct cpu_regs_state * regs);