#include "runtime/jit/jit_compilation_result.h"
#include "compiler/bytecode/bytecode_list.h"
#include "shared/types/function_object.h"
#include "shared.h"

static void optimize_bytecodes(struct bytecode_list *, struct function_profile_data profile);

struct __attribute__((weak)) jit_compilation_result try_jit_compile(struct function_object * function) {
    struct function_profile_data profile = function->state_as.profiling.profile_data;
    struct bytecode_list * bytecodes = create_bytecode_list(function->chunk);

    optimize_bytecodes(bytecodes, profile);
}

static void optimize_bytecodes(struct bytecode_list *, struct function_profile_data profile) {

}