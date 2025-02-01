#include "call_graph.h"

struct pending_call {
    struct call_graph_caller_data * parent;
    struct function_call * function_call;
};

static struct pending_call * create_pending_call(struct function_call * main, struct call_graph_caller_data * parent);
static void add_children(struct stack_list * stack, struct function_call * function_call, struct call_graph * parent);
static struct function_object * get_function_by_name(struct package * package, char * name);
static struct call_graph * get_call_graph_node(struct u64_hash_table *, struct function_call * caller,
        struct function_object * function_object, bool * call_graph_node_already_exists);

struct call_graph * create_call_graph(struct compilation_result * compilation_resul) {
    struct u64_hash_table functions_in_call_graph;
    init_u64_hash_table(&functions_in_call_graph, NATIVE_LOX_ALLOCATOR());
    struct stack_list pending;
    init_stack_list(&pending, NATIVE_LOX_ALLOCATOR());

    struct call_graph * call_graph = malloc(sizeof(struct call_graph));
    call_graph->function_object = compilation_resul->compiled_package->main_function;
    call_graph->package = compilation_resul->compiled_package;

    add_children(&pending, compilation_resul->compiled_package->main_function->function_calls, call_graph);

    while(!is_empty_stack_list(&pending)) {
        struct pending_call * pending_call = pop_stack_list(&pending);
        struct function_call * function_call_callee = pending_call->function_call;
        struct call_graph_caller_data * caller_data = pending_call->parent;
        struct package * function_package_callee = function_call_callee->package;
        char * function_name_callee = function_call_callee->function_name;
        struct function_object * function_object_callee = get_function_by_name(function_package_callee, function_name_callee);
        if (function_object_callee == NULL) { //Function not found, maybe it is a native function
            caller_data->call_graph = NULL;
            free(pending_call);
            continue;
        }

        bool callee_node_already_existed = false;

        struct call_graph * call_graph_node_callee = get_call_graph_node(&functions_in_call_graph, function_call_callee,
                function_object_callee, &callee_node_already_existed);

        caller_data->call_graph = call_graph_node_callee;

        if (!callee_node_already_existed) {
            add_children(&pending, function_object_callee->function_calls, call_graph_node_callee);
        }

        free(pending_call);
    }

    free_u64_hash_table(&functions_in_call_graph);
    free_stack_list(&pending);

    return call_graph;
}

static struct call_graph * get_call_graph_node(struct u64_hash_table * functions_in_call_graph, struct function_call * caller,
        struct function_object * function_object, bool * call_graph_node_already_exists) {
    struct call_graph * node_in_graph = get_u64_hash_table(functions_in_call_graph, (uint64_t) function_object);

    if(node_in_graph != NULL) {
        *call_graph_node_already_exists = true;
        return node_in_graph;
    }

    struct call_graph * new_node = malloc(sizeof(struct call_graph *));
    new_node->function_object = function_object;
    new_node->package = caller->package;
    *call_graph_node_already_exists = false;

    put_u64_hash_table(functions_in_call_graph, (uint64_t) function_object, new_node);

    return new_node;
}

static void add_children(
        struct stack_list * stack,
        struct function_call * function_call,
        struct call_graph * parent
) {
    int n_function_calls = get_n_function_calls(function_call);
    parent->children = malloc(sizeof(struct call_graph_caller_data) * n_function_calls);
    parent->n_childs = n_function_calls;

    struct function_call * current_function_call = function_call;
    for (int i = 0; i < n_function_calls; i++) {
        struct call_graph_caller_data * caller_data = malloc(sizeof(struct call_graph_caller_data));
        caller_data->call_bytecode_index = current_function_call->call_bytecode_index;
        caller_data->is_inlined = current_function_call->is_inlined;
        *(parent->children + i) = caller_data;

        push_stack_list(stack, create_pending_call(current_function_call, caller_data));

        current_function_call = current_function_call->prev;
    }
}

static struct function_object * get_function_by_name(struct package * package, char * function_name_chars) {
    struct string_object * function_name = copy_chars_to_string_object(function_name_chars, strlen(function_name_chars));

    lox_value_t function_lox_value = get_hash_table(&package->defined_functions, function_name);
    if(function_lox_value == NIL_VALUE) {
        return NULL; //If not found, it might be a native function
    }

    free(function_name);

    return (struct function_object *) AS_OBJECT(function_lox_value);
}

static struct pending_call * create_pending_call(struct function_call * main, struct call_graph_caller_data * parent) {
    struct pending_call * pending_call = malloc(sizeof(struct pending_call));
    pending_call->function_call = main;
    pending_call->parent = parent;
    return pending_call;
}

void free_recursive_call_graph(struct call_graph * call_graph) {
    struct call_graph_iterator iterator = iterate_call_graph(call_graph);

    while(has_next_call_graph(&iterator)){
        struct call_graph * to_free = next_call_graph(&iterator);

        for (int i = 0; i < to_free->n_childs; i++) {
            free(to_free->children[i]);
        }
        //TODO Ni puta idea, pero cada vez que se ejecuta free salta la señal de SIGTRAP
//        free(to_free);
    }

    free_call_graph_iterator(&iterator);
}

struct call_graph_iterator iterate_call_graph(struct call_graph * call_graph) {
    struct call_graph_iterator iterator;
    init_u64_hash_table(&iterator.already_checked, NATIVE_LOX_ALLOCATOR());
    init_stack_list(&iterator.pending, NATIVE_LOX_ALLOCATOR());
    init_stack_list(&iterator.parents, NATIVE_LOX_ALLOCATOR());
    push_stack_list(&iterator.pending, call_graph);

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

        if (contains_u64_hash_table(&iterator->already_checked, (uint64_t) pending_node)) {
            continue;
        }

        bool all_children_are_leafs = true;
        for (int i = 0; i < pending_node->n_childs; i++) {
            struct call_graph_caller_data * pending_node_child = pending_node->children[i];
            if(pending_node_child->call_graph != NULL && !contains_u64_hash_table(&iterator->already_checked, (uint64_t) pending_node_child->call_graph)){
                push_stack_list(&iterator->pending, pending_node_child->call_graph);
                all_children_are_leafs = false;
            }
        }

        if (all_children_are_leafs) {
            put_u64_hash_table(&iterator->already_checked, (uint64_t) pending_node, (void *) 0x01);
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