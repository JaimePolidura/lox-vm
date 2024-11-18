#include "runtime/jit/advanced/optimizations/const_folding_optimization.h"
#include "runtime/jit/advanced/creation/ssa_creator.h"
#include "runtime/jit/jit_compilation_result.h"
#include "runtime/threads/vm_thread.h"

#include "compiler/bytecode/bytecode_list.h"
#include "shared/types/function_object.h"

extern __thread struct vm_thread * self_thread;

struct jit_compilation_result __attribute__((weak)) try_jit_compile(struct function_object * function) {
    struct function_profile_data profile = function->state_as.profiling.profile_data;
    struct bytecode_list * bytecodes = create_bytecode_list(function->chunk);
    struct ssa_block * ssa_start_block = create_ssa_ir(self_thread->current_package, function, bytecodes);
}