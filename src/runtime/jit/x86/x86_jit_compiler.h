#pragma

#include "runtime/jit/x86/pending_path_jump.h"
#include "runtime/jit/x86/register_allocator.h"
#include "runtime/jit/x86/registers.h"
#include "runtime/jit/x86/opcodes.h"

#include "shared/utils/collections/u8_arraylist.h"
#include "shared/types/function_object.h"
#include "shared/os/os_utils.h"

#include "compiler/bytecode.h"

struct cpu_regs_state {
    uint64_t regs[N_MAX_REGISTERS];
};

struct jit_compiler {
    struct function_object * function_to_compile;

    //Next bytecode instruction to compiler
    uint8_t * pc;

    //Mapping of bytecode instructions to its compiled instructions index stored in native_compiled_code
    //This is used for knowing the relative offset when emitting assembly backward jumps
    uint16_t * compiled_bytecode_to_native_by_index;

    //Mapping of bytecode instructions to pending_path_jump, which will allow us to know if there is some
    //pending previous forward jump assembly offset to patch.
    struct pending_path_jump ** pending_jumps_to_patch;

    struct register_allocator register_allocator;

    struct u8_arraylist native_compiled_code;
};

jit_compiled jit_compile(struct function_object * function);

struct cpu_regs_state save_cpu_state();
void restore_cpu_state(struct cpu_regs_state * regs);