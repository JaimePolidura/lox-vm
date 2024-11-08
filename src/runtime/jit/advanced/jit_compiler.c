#include "runtime/jit/jit_compilation_result.h"
#include "compiler/bytecode/bytecode_list.h"
#include "shared/types/function_object.h"

static void inline_hot_functions(struct function_object *, struct bytecode_list *, struct function_profile_data *);

struct jit_compilation_result __attribute__((weak)) try_jit_compile(struct function_object * function) {
    struct function_profile_data profile = function->state_as.profiling.profile_data;
    struct bytecode_list * bytecodes = create_bytecode_list(function->chunk);

    inline_hot_functions(function, bytecodes, &profile);
}

static void inline_hot_functions(struct function_object *, struct bytecode_list *, struct function_profile_data *) {
    //TODO
}