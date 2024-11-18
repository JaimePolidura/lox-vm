#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/ssa_control_node.h"
#include "runtime/jit/advanced/utils/types.h"

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/package.h"

//This file will expoase the function "insert_ssa_ir_phis", given a bytecodelist, it will output the ssa graph ir
//without phis
#define READ_CONSTANT(function, bytecode) (function->chunk->constants.values[bytecode->as.u8])

typedef enum {
    EVAL_TYPE_SEQUENTIAL_CONTROL,
    EVAL_TYPE_JUMP_SEQUENTIAL_CONTROL,
    EVAL_TYPE_CONDITIONAL_JUMP_TRUE_BRANCH_CONTROL,
    EVAL_TYPE_CONDITIONAL_JUMP_FALSE_BRANCH_CONTROL,
} pending_evaluation_type_t;

struct pending_evaluate {
    struct bytecode_list * pending_bytecode;
    struct ssa_control_node * parent_ssa_node;
    pending_evaluation_type_t type;
};


struct ssa_no_phis_inserter {
    struct u64_hash_table control_nodes_by_bytecode;
    struct stack_list pending_evaluation;
    struct stack_list data_nodes_stack;
    struct stack_list package_stack;
};

static struct ssa_data_constant_node * create_constant_node(lox_value_t constant_value, struct bytecode_list *);
static void push_pending_evaluate(struct ssa_no_phis_inserter *, pending_evaluation_type_t, struct bytecode_list *, struct ssa_control_node *);
static void attatch_ssa_node_to_parent(pending_evaluation_type_t type, struct ssa_control_node * parent, struct ssa_control_node * child);
static struct bytecode_list * simplify_redundant_unconditional_jump_bytecodes(struct bytecode_list *bytecode_list);
static void map_data_nodes_bytecodes_to_control(
        struct u64_hash_table * control_ssa_nodes_by_bytecode,
        struct ssa_data_node * data_node,
        struct ssa_control_node * to_map_control
);
static struct ssa_no_phis_inserter * alloc_ssa_no_phis_inserter();
static void free_ssa_no_phis_inserter(struct ssa_no_phis_inserter *);
static void jump_if_false(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void jump(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void loop(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void pop(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void set_array_element(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void set_struct_field(struct function_object *, struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void set_global(struct function_object *, struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void exit_monitor_explicit(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void exit_monitor_opcode(struct function_object *, struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void enter_monitor_explicit(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void enter_monitor_opcode(struct function_object *, struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void return_opcode(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void print(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void set_local(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void get_local(struct function_object *, struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void call(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void get_global(struct function_object *, struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void initialize_array(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void initialize_struct(struct function_object *, struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void get_struct_field(struct function_object *, struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void get_array_element(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void unary(struct ssa_no_phis_inserter *, struct pending_evaluate *);
static void binary(struct ssa_no_phis_inserter *, struct pending_evaluate *);

struct ssa_control_node * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
) {
    struct ssa_no_phis_inserter * inserter = alloc_ssa_no_phis_inserter();

    push_stack_list(&inserter->package_stack, package);

    struct ssa_control_start_node * start_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_START, struct ssa_control_start_node);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, start_function_bytecode, &start_node->control);

    while (!is_empty_stack_list(&inserter->pending_evaluation)) {
        struct pending_evaluate * to_evaluate = pop_stack_list(&inserter->pending_evaluation);

        if (contains_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode)) {
            struct ssa_control_node * already_evaluted_node = get_u64_hash_table(&inserter->control_nodes_by_bytecode,
                    (uint64_t) to_evaluate->pending_bytecode);
            attatch_ssa_node_to_parent(to_evaluate->type, to_evaluate->parent_ssa_node, already_evaluted_node);
            continue;
        }

        switch (to_evaluate->pending_bytecode->bytecode) {
            case OP_JUMP_IF_FALSE: jump_if_false(inserter, to_evaluate); break;
            case OP_JUMP: jump(inserter, to_evaluate); break;
            case OP_LOOP: loop(inserter, to_evaluate); break;
            case OP_POP: pop(inserter, to_evaluate); break;
            case OP_SET_ARRAY_ELEMENT: set_array_element(inserter, to_evaluate); break;
            case OP_SET_STRUCT_FIELD: set_struct_field(function, inserter, to_evaluate); break;
            case OP_SET_GLOBAL: set_global(function, inserter, to_evaluate); break;
            case OP_EXIT_MONITOR_EXPLICIT: exit_monitor_explicit(inserter, to_evaluate); break;
            case OP_EXIT_MONITOR: exit_monitor_opcode(function, inserter, to_evaluate); break;
            case OP_ENTER_MONITOR_EXPLICIT: enter_monitor_explicit(inserter, to_evaluate); break;
            case OP_ENTER_MONITOR: enter_monitor_opcode(function, inserter, to_evaluate); break;
            case OP_RETURN: return_opcode(inserter, to_evaluate); break;
            case OP_PRINT: print(inserter, to_evaluate); break;
            case OP_SET_LOCAL: set_local(inserter, to_evaluate); break;

            //Expressions -> control nodes
            case OP_GET_LOCAL: get_local(function, inserter, to_evaluate); break;
            case OP_CALL: call(inserter, to_evaluate); break;
            case OP_GET_GLOBAL: get_global(function, inserter, to_evaluate); break;
            case OP_INITIALIZE_ARRAY: initialize_array(inserter, to_evaluate); break;
            case OP_INITIALIZE_STRUCT: initialize_struct(function, inserter, to_evaluate); break;
            case OP_GET_STRUCT_FIELD: get_struct_field(function, inserter, to_evaluate); break;
            case OP_GET_ARRAY_ELEMENT: get_array_element(inserter, to_evaluate); break;
            case OP_NEGATE:
            case OP_NOT: unary(inserter, to_evaluate); break;
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_GREATER:
            case OP_LESS:
            case OP_EQUAL: binary(inserter, to_evaluate); break;
            case OP_CONSTANT: {
                push_stack_list(&inserter->data_nodes_stack, create_constant_node(READ_CONSTANT(function, to_evaluate->pending_bytecode), to_evaluate->pending_bytecode));
                push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
                break;
            }
            case OP_FALSE: {
                push_stack_list(&inserter->data_nodes_stack, create_constant_node(TAG_FALSE, to_evaluate->pending_bytecode));
                push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
                break;
            }
            case OP_TRUE: {
                push_stack_list(&inserter->data_nodes_stack, create_constant_node(TAG_TRUE, to_evaluate->pending_bytecode));
                push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
                break;
            }
            case OP_NIL: {
                push_stack_list(&inserter->data_nodes_stack, create_constant_node(TAG_NIL, to_evaluate->pending_bytecode));
                push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
                break;
            }
            case OP_FAST_CONST_8: {
                push_stack_list(&inserter->data_nodes_stack, create_constant_node(AS_NUMBER(to_evaluate->pending_bytecode->as.u8), to_evaluate->pending_bytecode));
                push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
                break;
            }
            case OP_FAST_CONST_16: {
                push_stack_list(&inserter->data_nodes_stack, create_constant_node(AS_NUMBER(to_evaluate->pending_bytecode->as.u16), to_evaluate->pending_bytecode));
                push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
                break;
            }
            case OP_CONST_1: {
                push_stack_list(&inserter->data_nodes_stack, create_constant_node(AS_NUMBER(1), to_evaluate->pending_bytecode));
                push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
                break;
            }
            case OP_CONST_2: {
                push_stack_list(&inserter->data_nodes_stack, create_constant_node(AS_NUMBER(2), to_evaluate->pending_bytecode));
                push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
                break;
            }
            case OP_PACKAGE_CONST: {
                lox_value_t package_lox_value = READ_CONSTANT(function, to_evaluate->pending_bytecode);
                struct package_object * package_const = (struct package_object *) AS_OBJECT(package_lox_value);

                push_stack_list(&inserter->package_stack, package_const->package);
                push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
                break;
            }
            case OP_ENTER_PACKAGE: break;
            case OP_EXIT_PACKAGE: {
                pop_stack_list(&inserter->package_stack);
                break;
            }
            case OP_DEFINE_GLOBAL:
            case OP_NO_OP:
            case OP_EOF:
                break;
        }

        free(to_evaluate);
    }

    free_ssa_no_phis_inserter(inserter);

    return &start_node->control;
}

static void binary(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct ssa_data_binary_node * binaryt_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_BINARY, struct ssa_data_binary_node, to_evaluate->pending_bytecode
    );
    struct ssa_data_node * right = pop_stack_list(&inserter->data_nodes_stack);
    struct ssa_data_node * left = pop_stack_list(&inserter->data_nodes_stack);
    binaryt_node->data.produced_type = PROFILE_DATA_TYPE_BOOLEAN;
    binaryt_node->operand = to_evaluate->pending_bytecode->bytecode;
    binaryt_node->right = right;
    binaryt_node->left = left;

    push_stack_list(&inserter->data_nodes_stack, binaryt_node);
    push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
}

static void unary(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct ssa_data_unary_node * unary_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_UNARY, struct ssa_data_unary_node, to_evaluate->pending_bytecode
    );
    unary_node->operator_type = to_evaluate->pending_bytecode->bytecode == OP_NEGATE ? UNARY_OPERATION_TYPE_NEGATION : UNARY_OPERATION_TYPE_NOT;
    unary_node->operand = pop_stack_list(&inserter->data_nodes_stack);
    unary_node->data.produced_type = unary_node->operand->produced_type;

    push_stack_list(&inserter->data_nodes_stack, unary_node);
    push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
}

static void get_array_element(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct ssa_data_get_array_element_node * get_array_element_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT, struct ssa_data_get_array_element_node, to_evalute->pending_bytecode
    );
    get_array_element_node->instance = pop_stack_list(&inserter->data_nodes_stack);
    get_array_element_node->data.produced_type = PROFILE_DATA_TYPE_ANY;
    get_array_element_node->index = to_evalute->pending_bytecode->as.u16;

    push_stack_list(&inserter->data_nodes_stack, get_array_element_node);
    push_pending_evaluate(inserter, to_evalute->type, to_evalute->pending_bytecode->next, to_evalute->parent_ssa_node);
}

static void get_struct_field(
        struct function_object * function,
        struct ssa_no_phis_inserter * inserter,
        struct pending_evaluate * to_evalute
) {
    struct ssa_data_get_struct_field_node * get_struct_field_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD, struct ssa_data_get_struct_field_node, to_evalute->pending_bytecode
    );
    get_struct_field_node->data.produced_type = PROFILE_DATA_TYPE_ANY;
    get_struct_field_node->instance_node = pop_stack_list(&inserter->data_nodes_stack);
    get_struct_field_node->field_name = AS_STRING_OBJECT(READ_CONSTANT(function, to_evalute->pending_bytecode));

    push_stack_list(&inserter->data_nodes_stack, get_struct_field_node);
    push_pending_evaluate(inserter, to_evalute->type, to_evalute->pending_bytecode->next, to_evalute->parent_ssa_node);
}

static void initialize_array(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct ssa_data_initialize_array_node * initialize_array_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY, struct ssa_data_initialize_array_node, to_evaluate->pending_bytecode
    );
    bool empty_initialization = to_evaluate->pending_bytecode->as.initialize_array.is_emtpy_initializaion;
    uint16_t n_elements = to_evaluate->pending_bytecode->as.initialize_array.n_elements;

    initialize_array_node->data.produced_type = PROFILE_DATA_TYPE_OBJECT;
    initialize_array_node->empty_initialization = empty_initialization;
    initialize_array_node->n_elements = n_elements;
    if(!empty_initialization){
        initialize_array_node->elememnts_node = malloc(sizeof(struct ssa_data_node *) * n_elements);
    }
    for(int i = n_elements - 1; i >= 0 && !empty_initialization; i--){
        initialize_array_node->elememnts_node[i] = pop_stack_list(&inserter->data_nodes_stack);
    }

    push_stack_list(&inserter->data_nodes_stack, initialize_array_node);
    push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
}

static void initialize_struct(
        struct function_object * function,
        struct ssa_no_phis_inserter * inserter,
        struct pending_evaluate * to_evalute
) {
    struct ssa_data_initialize_struct_node * initialize_struct_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT, struct ssa_data_initialize_struct_node, to_evalute->pending_bytecode
    );
    struct struct_definition_object * definition = (struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(function, to_evalute->pending_bytecode));
    initialize_struct_node->data.produced_type = PROFILE_DATA_TYPE_OBJECT;
    initialize_struct_node->fields_nodes = malloc(sizeof(struct struct_definition_object *) * definition->n_fields);
    initialize_struct_node->definition = definition;
    for (int i = definition->n_fields - 1; i >= 0; i--) {
        initialize_struct_node->fields_nodes[i] = pop_stack_list(&inserter->data_nodes_stack);
    }

    push_stack_list(&inserter->data_nodes_stack, initialize_struct_node);
    push_pending_evaluate(inserter, to_evalute->type, to_evalute->pending_bytecode->next, to_evalute->parent_ssa_node);
}

static void get_global(
        struct function_object * function,
        struct ssa_no_phis_inserter * inserter,
        struct pending_evaluate * to_evalute
) {
    struct package * global_variable_package = peek_stack_list(&inserter->data_nodes_stack);
    struct string_object * global_variable_name = AS_STRING_OBJECT(READ_CONSTANT(function, to_evalute->pending_bytecode));
    //If the global variable is constant, we will return a CONST_NODE instead of GET_GLOBAL node
    if(contains_trie(&global_variable_package->const_global_variables_names, global_variable_name->chars, global_variable_name->length)) {
        lox_value_t constant_value;
        get_hash_table(&global_variable_package->global_variables, global_variable_name, &constant_value);
        struct ssa_data_constant_node * constant_node = create_constant_node(constant_value, to_evalute->pending_bytecode);
        push_stack_list(&inserter->data_nodes_stack, constant_node);
    } else { //Non constant global variable
        struct ssa_data_get_global_node * get_global_node = ALLOC_SSA_DATA_NODE(
                SSA_DATA_NODE_TYPE_GET_GLOBAL, struct ssa_data_get_global_node, to_evalute->pending_bytecode
        );
        get_global_node->data.produced_type = PROFILE_DATA_TYPE_ANY;
        get_global_node->package = global_variable_package;
        get_global_node->name = global_variable_name;

        push_stack_list(&inserter->data_nodes_stack, get_global_node);
    }

    push_pending_evaluate(inserter, to_evalute->type, to_evalute->pending_bytecode->next, to_evalute->parent_ssa_node);

}

static void call(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct ssa_control_function_call_node * call_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_CALL, struct ssa_control_function_call_node, to_evaluate->pending_bytecode
    );
    bool is_paralell = to_evaluate->pending_bytecode->as.pair.u8_2;
    uint8_t n_args = to_evaluate->pending_bytecode->as.pair.u8_1;
    struct ssa_data_node * function_to_call = peek_n_stack_list(&inserter->data_nodes_stack, n_args);

    call_node->data.produced_type = PROFILE_DATA_TYPE_ANY;
    call_node->function = function_to_call;
    call_node->is_parallel = is_paralell;
    call_node->n_arguments = n_args;
    call_node->arguments = malloc(sizeof(struct ssa_data_node *) * n_args);
    for(int i = n_args; i > 0; i--){
        call_node->arguments[i - 1] = pop_stack_list(&inserter->data_nodes_stack);
    }
    //Remove function object in the stack
    pop_stack_list(&inserter->data_nodes_stack);
    push_stack_list(&inserter->data_nodes_stack, call_node);
    push_pending_evaluate(inserter, to_evaluate->type, to_evaluate->pending_bytecode->next, to_evaluate->parent_ssa_node);
}

static void get_local(
        struct function_object * function,
        struct ssa_no_phis_inserter * inserter,
        struct pending_evaluate * to_evalute
) {
    struct ssa_data_get_local_node * get_local_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_LOCAL, struct ssa_data_get_local_node, to_evalute->pending_bytecode
    );

    //Pending initialize
    int local_number = to_evalute->pending_bytecode->as.u8;
    get_local_node->local_number = local_number;
    get_local_node->data.produced_type = get_type_by_local_function_profile_data(&function->state_as.profiling.profile_data, local_number);

    push_stack_list(&inserter->data_nodes_stack, get_local_node);
    push_pending_evaluate(inserter, to_evalute->type, to_evalute->pending_bytecode->next, to_evalute->parent_ssa_node);
}

static void set_local(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct ssa_control_set_local_node * set_local_node = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTORL_NODE_TYPE_SET_LOCAL, struct ssa_control_set_local_node
    );
    struct ssa_data_node * new_local_value = pop_stack_list(&inserter->data_nodes_stack);
    set_local_node->local_number = to_evaluate->pending_bytecode->as.u8;
    set_local_node->new_local_value = new_local_value;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, new_local_value, &set_local_node->control);
    attatch_ssa_node_to_parent(to_evaluate->type, to_evaluate->parent_ssa_node, &set_local_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evaluate->pending_bytecode->next, &set_local_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode, set_local_node);
}

static void print(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct ssa_control_print_node * print_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_PRINT, struct ssa_control_print_node);
    struct ssa_data_node * print_value = pop_stack_list(&inserter->data_nodes_stack);

    print_node->data = print_value;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, print_value, &print_node->control);
    attatch_ssa_node_to_parent(to_evaluate->type, to_evaluate->parent_ssa_node, &print_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evaluate->pending_bytecode->next, &print_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode, print_node);
}

static void return_opcode(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct ssa_control_return_node * return_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_RETURN, struct ssa_control_return_node);
    struct ssa_data_node * return_value = pop_stack_list(&inserter->data_nodes_stack);

    return_node->data = return_value;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, return_value, &return_node->control);
    attatch_ssa_node_to_parent(to_evalute->type, to_evalute->parent_ssa_node, &return_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, return_node);
}

static void enter_monitor_opcode(
        struct function_object * function,
        struct ssa_no_phis_inserter * inserter,
        struct pending_evaluate * to_evaluate
) {
    monitor_number_t monitor_number = to_evaluate->pending_bytecode->as.u8;
    struct monitor * monitor = &function->monitors[monitor_number];
    struct ssa_control_enter_monitor_node * enter_monitor_node = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_ENTER_MONITOR, struct ssa_control_enter_monitor_node
    );

    enter_monitor_node->monitor = monitor;

    attatch_ssa_node_to_parent(to_evaluate->type, to_evaluate->parent_ssa_node, &enter_monitor_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evaluate->pending_bytecode->next, &enter_monitor_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode, enter_monitor_node);
}

static void enter_monitor_explicit(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct monitor * monitor = (struct monitor *) to_evalute->pending_bytecode->as.u64;
    struct ssa_control_enter_monitor_node * enter_monitor_node = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_ENTER_MONITOR, struct ssa_control_enter_monitor_node
    );

    enter_monitor_node->monitor = monitor;

    attatch_ssa_node_to_parent(to_evalute->type, to_evalute->parent_ssa_node, &enter_monitor_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evalute->pending_bytecode->next, &enter_monitor_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, enter_monitor_node);
}

static void exit_monitor_opcode(
        struct function_object * function,
        struct ssa_no_phis_inserter * inserter,
        struct pending_evaluate * to_evalute
) {
    monitor_number_t monitor_number = to_evalute->pending_bytecode->as.u8;
    struct monitor * monitor = &function->monitors[monitor_number];
    struct ssa_control_exit_monitor_node * exit_monitor_node = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_EXIT_MONITOR, struct ssa_control_exit_monitor_node
    );

    exit_monitor_node->monitor = monitor;

    attatch_ssa_node_to_parent(to_evalute->type, to_evalute->parent_ssa_node, &exit_monitor_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evalute->pending_bytecode->next, &exit_monitor_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, exit_monitor_node);
}

static void exit_monitor_explicit(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct monitor * monitor = (struct monitor *) to_evalute->pending_bytecode->as.u64;
    struct ssa_control_exit_monitor_node * exit_monitor_node = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_EXIT_MONITOR, struct ssa_control_exit_monitor_node
    );

    exit_monitor_node->monitor = monitor;

    attatch_ssa_node_to_parent(to_evalute->type, to_evalute->parent_ssa_node, &exit_monitor_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evalute->pending_bytecode->next, &exit_monitor_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, exit_monitor_node);
}

static void set_global(
        struct function_object * function,
        struct ssa_no_phis_inserter * inserter,
        struct pending_evaluate * to_evaluate
) {
    struct ssa_control_set_global_node * set_global_node = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTORL_NODE_TYPE_SET_GLOBAL, struct ssa_control_set_global_node
    );

    set_global_node->name = AS_STRING_OBJECT(READ_CONSTANT(function, to_evaluate->pending_bytecode));
    set_global_node->value_node = pop_stack_list(&inserter->data_nodes_stack);
    set_global_node->package = peek_stack_list(&inserter->package_stack);

    attatch_ssa_node_to_parent(to_evaluate->type, to_evaluate->parent_ssa_node, &set_global_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evaluate->pending_bytecode->next, &set_global_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode, set_global_node);
}

static void set_struct_field(
        struct function_object * function,
        struct ssa_no_phis_inserter * inserter,
        struct pending_evaluate * to_evalute
) {
    struct ssa_control_set_struct_field_node * set_struct_field = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD, struct ssa_control_set_struct_field_node
    );
    struct ssa_data_node * field_value = pop_stack_list(&inserter->data_nodes_stack);
    struct ssa_data_node * instance = pop_stack_list(&inserter->data_nodes_stack);

    set_struct_field->field_name = AS_STRING_OBJECT(READ_CONSTANT(function, to_evalute->pending_bytecode));
    set_struct_field->new_field_value = field_value;
    set_struct_field->instance = instance;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, field_value, &set_struct_field->control);
    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, instance, &set_struct_field->control);
    attatch_ssa_node_to_parent(to_evalute->type, to_evalute->parent_ssa_node, &set_struct_field->control);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evalute->pending_bytecode->next, &set_struct_field->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, set_struct_field);
}

static void set_array_element(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct ssa_control_set_array_element_node * set_arrary_element_node = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT, struct ssa_control_set_array_element_node
    );
    struct ssa_data_node * instance = pop_stack_list(&inserter->data_nodes_stack);
    struct ssa_data_node * new_element = pop_stack_list(&inserter->data_nodes_stack);

    set_arrary_element_node->index = to_evalute->pending_bytecode->as.u16;
    set_arrary_element_node->new_element_value = new_element;
    set_arrary_element_node->array = instance;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, instance, &set_arrary_element_node->control);
    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, new_element, &set_arrary_element_node->control);
    attatch_ssa_node_to_parent(to_evalute->type, to_evalute->parent_ssa_node, &set_arrary_element_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evalute->pending_bytecode->next, &set_arrary_element_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, set_arrary_element_node);
}

//Expression statements
static void pop(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    if(!is_empty_stack_list(&inserter->data_nodes_stack)){
        struct ssa_data_node * data_node = pop_stack_list(&inserter->data_nodes_stack);
        struct ssa_control_data_node * control_data_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_DATA, struct ssa_control_data_node);

        control_data_node->data = data_node;

        map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, data_node, &control_data_node->control);
        attatch_ssa_node_to_parent(to_evalute->type, to_evalute->parent_ssa_node, &control_data_node->control);
        push_pending_evaluate(inserter, EVAL_TYPE_SEQUENTIAL_CONTROL, to_evalute->pending_bytecode->next, &control_data_node->control);
        put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, control_data_node);
    }
}

static void loop(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct ssa_control_loop_jump_node * loop_jump_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_LOOP_JUMP, struct ssa_control_loop_jump_node);
    struct bytecode_list * to_jump_bytecode = to_evalute->pending_bytecode->as.jump;
    struct ssa_control_node * to_jump_ssa_node = get_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_jump_bytecode);

    if(to_jump_ssa_node->type == SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP){
        struct ssa_control_conditional_jump_node * loop_condition_node = (struct ssa_control_conditional_jump_node *) to_jump_ssa_node;
        loop_condition_node->loop_condition = true;
    }

    attatch_ssa_node_to_parent(to_evalute->type, to_evalute->parent_ssa_node, &loop_jump_node->control);
    loop_jump_node->control.next.next = to_jump_ssa_node;
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, loop_jump_node);
}

static void jump(struct ssa_no_phis_inserter * insterter, struct pending_evaluate * to_evalute) {
    //OP_JUMP are translated to edges in the ssa ir graph
    struct bytecode_list * to_jump_bytecode = simplify_redundant_unconditional_jump_bytecodes(to_evalute->pending_bytecode->as.jump);
    push_pending_evaluate(insterter, EVAL_TYPE_JUMP_SEQUENTIAL_CONTROL, to_jump_bytecode, to_evalute->parent_ssa_node);
}

static void jump_if_false(struct ssa_no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct ssa_control_conditional_jump_node * cond_jump_node = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP, struct ssa_control_conditional_jump_node
    );
    struct ssa_data_node * condition = pop_stack_list(&inserter->data_nodes_stack);

    cond_jump_node->condition = condition;
    struct bytecode_list * false_branch_bytecode = to_evalute->pending_bytecode->as.jump;
    struct bytecode_list * true_branch_bytecode = to_evalute->pending_bytecode->next;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, condition, &cond_jump_node->control);
    attatch_ssa_node_to_parent(to_evalute->type, to_evalute->parent_ssa_node, &cond_jump_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_CONDITIONAL_JUMP_FALSE_BRANCH_CONTROL, false_branch_bytecode, &cond_jump_node->control);
    push_pending_evaluate(inserter, EVAL_TYPE_CONDITIONAL_JUMP_TRUE_BRANCH_CONTROL, true_branch_bytecode, &cond_jump_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, cond_jump_node);
}

static struct ssa_data_constant_node * create_constant_node(lox_value_t constant_value, struct bytecode_list * bytecode) {
    struct ssa_data_constant_node * constant_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_CONSTANT, struct ssa_data_constant_node, bytecode
    );

    constant_node->constant_lox_value = constant_value;

    switch (get_lox_type(constant_value)) {
        case VAL_BOOL:
            constant_node->value_as.boolean = AS_BOOL(constant_value);
            constant_node->data.produced_type = PROFILE_DATA_TYPE_BOOLEAN;
            break;

        case VAL_NIL: {
            constant_node->value_as.nil = NULL;
            constant_node->data.produced_type = PROFILE_DATA_TYPE_NIL;
            break;
        }

        case VAL_NUMBER: {
            if(has_decimals(AS_NUMBER(constant_value))){
                constant_node->value_as.f64 = AS_NUMBER(constant_value);
                constant_node->data.produced_type = PROFILE_DATA_TYPE_F64;
            } else {
                constant_node->value_as.i64 = (int64_t) constant_value;
                constant_node->data.produced_type = PROFILE_DATA_TYPE_I64;
            }
            break;
        }

        case VAL_OBJ: {
            if(IS_STRING(constant_value)){
                constant_node->value_as.string = AS_STRING_OBJECT(constant_value);
                constant_node->data.produced_type = PROFILE_DATA_TYPE_STRING;
            } else {
                constant_node->value_as.object = AS_OBJECT(constant_value);
                constant_node->data.produced_type = PROFILE_DATA_TYPE_OBJECT;
            }
            break;
        }
    }

    return constant_node;
}

static void push_pending_evaluate(
        struct ssa_no_phis_inserter * inserter,
        pending_evaluation_type_t type,
        struct bytecode_list * pending_bytecode,
        struct ssa_control_node * parent_ssa_node
) {
    if (pending_bytecode != NULL) {
        struct pending_evaluate * pending_evalutaion = malloc(sizeof(struct pending_evaluate));
        pending_evalutaion->pending_bytecode = pending_bytecode;
        pending_evalutaion->parent_ssa_node = parent_ssa_node;
        pending_evalutaion->type = type;
        push_stack_list(&inserter->pending_evaluation, pending_evalutaion);
    }
}

static void attatch_ssa_node_to_parent(
        pending_evaluation_type_t type,
        struct ssa_control_node * parent,
        struct ssa_control_node * child
) {
    child->prev = parent;

    switch (type) {
        case EVAL_TYPE_JUMP_SEQUENTIAL_CONTROL:
            parent->jumps_to_next_node = true;
        case EVAL_TYPE_SEQUENTIAL_CONTROL:
            parent->next.next = child;
            break;
        case EVAL_TYPE_CONDITIONAL_JUMP_TRUE_BRANCH_CONTROL:
            parent->next.branch.true_branch = child;
            break;
        case EVAL_TYPE_CONDITIONAL_JUMP_FALSE_BRANCH_CONTROL:
            parent->next.branch.false_branch = child;
            break;
    }
}

//This function simplifies the unconditional jump instructions,
//For example: given OP_JUMP_a which points to OP_JUMP_b which points to OP_PRINT,
//It will return OP_PRINT
//This redundant jump bytecodes will appear in neseted if statements
static struct bytecode_list * simplify_redundant_unconditional_jump_bytecodes(struct bytecode_list * start_jump) {
    struct bytecode_list * current = start_jump;
    while(current->bytecode == OP_JUMP){
        current = current->as.jump;
    }
    return current;
}

struct map_data_nodes_bytecodes_to_control_struct {
    struct u64_hash_table * control_ssa_nodes_by_bytecode;
    struct ssa_control_node * to_map_control;
};

static void map_data_nodes_bytecodes_to_control_consumer(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * current_node,
        void * extra
) {
    struct map_data_nodes_bytecodes_to_control_struct * consumer_struct = extra;

    //Add data_node
    put_u64_hash_table(consumer_struct->control_ssa_nodes_by_bytecode, (uint64_t) current_node->original_bytecode,
                       consumer_struct->to_map_control);
}

//This function will map the control node bytecode to the to_map_control control node
//Example: Given OP_CONST_1, OP_CONST_2, OP_ADD, OP_PRINT
//The first 3 bytecodes will point to OP_PRINT
static void map_data_nodes_bytecodes_to_control(
        struct u64_hash_table * control_ssa_nodes_by_bytecode,
        struct ssa_data_node * data_node,
        struct ssa_control_node * to_map_control
) {
    struct map_data_nodes_bytecodes_to_control_struct consumer_struct = (struct map_data_nodes_bytecodes_to_control_struct) {
            .control_ssa_nodes_by_bytecode = control_ssa_nodes_by_bytecode,
            .to_map_control = to_map_control,
    };

    for_each_ssa_data_node(data_node, NULL, &consumer_struct, map_data_nodes_bytecodes_to_control_consumer);
}

static struct ssa_no_phis_inserter * alloc_ssa_no_phis_inserter() {
    struct ssa_no_phis_inserter * ssa_no_phis_inserter = malloc(sizeof(struct ssa_no_phis_inserter));
    init_u64_hash_table(&ssa_no_phis_inserter->control_nodes_by_bytecode);
    init_stack_list(&ssa_no_phis_inserter->pending_evaluation);
    init_stack_list(&ssa_no_phis_inserter->data_nodes_stack);
    init_stack_list(&ssa_no_phis_inserter->package_stack);
    return ssa_no_phis_inserter;
}

static void free_ssa_no_phis_inserter(struct ssa_no_phis_inserter * inserter) {
    free_u64_hash_table(&inserter->control_nodes_by_bytecode);
    free_stack_list(&inserter->pending_evaluation);
    free_stack_list(&inserter->data_nodes_stack);
    free_stack_list(&inserter->package_stack);
}