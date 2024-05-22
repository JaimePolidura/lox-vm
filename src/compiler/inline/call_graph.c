#include "call_graph.h"

struct pending_call {
    struct call_graph_caller_data * parent;
    struct function_call * function_call;
};

static struct pending_call * create_pending_call(struct function_call * main, struct call_graph_caller_data * parent);
static void add_children(struct stack_list * stack, struct function_call * function_call, struct call_graph * parent);
static struct function_object * get_function_by_name(struct package * package, char * name);

struct call_graph * create_call_graph(struct compilation_result * compilation_resul) {
    struct stack_list pending;
    init_stack_list(&pending);

    struct call_graph * call_graph = malloc(sizeof(struct call_graph));
    call_graph->function_object = compilation_resul->compiled_package->main_function;
    call_graph->package = compilation_resul->compiled_package;
    call_graph->scope = SCOPE_PACKAGE;

    add_children(&pending, compilation_resul->compiled_package->main_function->function_calls, call_graph);

    while(!is_empty_stack_list(&pending)) {
        struct pending_call * pending_call = pop_stack_list(&pending);
        struct function_call * callee_function_call = pending_call->function_call;
        struct call_graph_caller_data * caller_data = pending_call->parent;
        struct package * function_package_callee = callee_function_call->package;
        char * function_name_callee = callee_function_call->function_name;

        struct function_object * function_object_callee = get_function_by_name(function_package_callee, function_name_callee);

        struct call_graph * callee = malloc(sizeof(struct call_graph *));
        callee->scope = callee_function_call->function_scope;
        callee->function_object = function_object_callee;
        callee->package = callee_function_call->package;
        caller_data->call_graph = callee;

        add_children(&pending, function_object_callee->function_calls, callee);
    }

    return call_graph;
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
    lox_value_t function_lox_value;
    struct string_object * function_name = copy_chars_to_string_object(function_name_chars, strlen(function_name_chars));
    get_hash_table(&package->defined_functions, function_name, &function_lox_value);
    free(function_name);

    return (struct function_object *) AS_OBJECT(function_lox_value);
}

static struct pending_call * create_pending_call(struct function_call * main, struct call_graph_caller_data * parent) {
    struct pending_call * pending_call = malloc(sizeof(struct pending_call));
    pending_call->function_call = main;
    pending_call->parent = parent;
    return pending_call;
}