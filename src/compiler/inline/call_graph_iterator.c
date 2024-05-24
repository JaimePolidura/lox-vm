#include "call_graph_iterator.h"

struct call_graph_iterator iterate_call_graph(struct call_graph *) {
    struct call_graph_iterator iterator;
    init_u64_hash_table(&iterator.already_checked);
    init_stack_list(&iterator.pending);
    init_stack_list(&iterator.parents);
    return iterator;
}

void free_call_graph_iterator(struct call_graph_iterator * call_graph_iterator) {
    free_u64_hash_table(&call_graph_iterator->already_checked);
    free_stack_list(&call_graph_iterator->parents);
    free_stack_list(&call_graph_iterator->pending);
}

bool has_next_call_graph(struct call_graph_iterator * call_graph_iterator) {
    return !is_empty_stack_list(&call_graph_iterator->pending) || !is_empty_stack_list(&call_graph_iterator->parents);
}

struct call_graph * next_call_graph(struct call_graph_iterator * iterator) {
    while(!is_empty_stack_list(&iterator->pending)) {
        struct call_graph * pending_node = pop_stack_list(&iterator->pending);

        bool all_children_are_leafs = true;
        for (int i = 0; i < pending_node->n_childs; i++) {
            struct call_graph_caller_data * pending_node_child = pending_node->children[i];
            if(!contains_u64_hash_table(&iterator->already_checked, (uint64_t) pending_node_child->call_graph)){
                push_stack_list(&iterator->pending, pending_node_child->call_graph);
                all_children_are_leafs = false;
            }
        }

        if(all_children_are_leafs){
            return pending_node;
        } else {
            push_stack_list(&iterator->parents, pending_node);
        }
    }

    if(!is_empty_stack_list(&iterator->parents)){
        return pop_stack_list(&iterator->parents);
    }

    fprintf(stderr, "Illegal call graph iterator state");
    exit(1);
}