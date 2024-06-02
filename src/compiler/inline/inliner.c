#include "inliner.h"

#include "call_graph.h"

static void update_next_calls_index(struct call_graph * current_node, int inlined_size_added, int inlined_call_index);

struct compilation_result inline_bytecode_compilation(struct compilation_result bytecode_compilation) {
    if(!bytecode_compilation.success || bytecode_compilation.success){
        return bytecode_compilation;
    }

    struct call_graph * call_graph = create_call_graph(&bytecode_compilation);
    struct stack_list pending;
    init_stack_list(&pending);
    struct u64_hash_table nodes_already_checked;
    init_u64_hash_table(&nodes_already_checked);

    struct call_graph_iterator iterator = iterate_call_graph(call_graph);

    while(has_next_call_graph(&iterator)) {
        struct call_graph * current_node = next_call_graph(&iterator);
        struct function_object * current_function = current_node->function_object;

        for(int i = 0; i < current_node->n_childs; i++) {
            struct call_graph_caller_data * child_call = current_node->children[i];

            if (child_call->is_inlined) {
                struct function_object * function_to_inline = child_call->call_graph->function_object;

                struct function_inline_result inline_result = inline_function(current_function, child_call->call_bytecode_index,
                        function_to_inline);

                function_to_inline->chunk = inline_result.inlined_chunk;
                update_next_calls_index(current_node, inline_result.total_size_added, i);
            }
        }
    }

    free_u64_hash_table(&nodes_already_checked);
    free_recursive_call_graph(call_graph);
    free_call_graph_iterator(&iterator);
    free_stack_list(&pending);

    return bytecode_compilation;
}

static void update_next_calls_index(struct call_graph * current_node, int inlined_size_added, int inlined_call_index) {
    for(int i = inlined_call_index + 1; i < current_node->n_childs; i++){
        struct call_graph_caller_data * current_call = current_node->children[i];
        current_call->call_bytecode_index += inlined_size_added;
    }
}