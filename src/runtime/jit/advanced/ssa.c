#include "shared/utils/collections/stack_list.h"
#include "runtime/jit/jit_compilation_result.h"
#include "compiler/bytecode/bytecode_list.h"
#include "shared/types/function_object.h"
#include "shared/package.h"

void create_ssa(
        struct package * package,
        struct bytecode_list * list
) {
    struct stack_list * stack = alloc_stack_list();

    for (struct bytecode_list * current = list; list != NULL; list = list->next) {
        
    }
}