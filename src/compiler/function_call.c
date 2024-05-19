#include "function_call.h"

void init_function_calls(struct function_call * function_call) {
    function_call->package = NULL;
    function_call->function_name = NULL;
    function_call->prev = NULL;
    function_call->is_inlined = false;
    function_call->call_bytecode_index = 0;
}

struct function_call * alloc_function_call() {
    struct function_call * function_call = malloc(sizeof(struct function_call));
    init_function_calls(function_call);
    return function_call;
}
