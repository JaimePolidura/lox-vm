#include "runtime/jit/advanced/creation/lox_ir_creator.h"
#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/lox_ir_options.h"
#include "runtime/threads/vm_thread.h"

#include "compiler/bytecode/bytecode_list.h"
#include "shared/types/function_object.h"

extern __thread struct vm_thread * self_thread;

struct jit_compilation_result __attribute__((weak)) try_jit_compile(struct function_object * function) {
    struct function_profile_data profile = function->state_as.profiling.profile_data;
    struct bytecode_list * bytecodes = create_bytecode_list(function->chunk, NATIVE_LOX_ALLOCATOR());

    exit(-2);
}