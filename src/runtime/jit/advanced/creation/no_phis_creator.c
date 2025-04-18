#include "no_phis_creator.h"

//This file will expoase the function "insert_lox_ir_phis", given a bytecodelist, it will output the lox graph ir
//without phis
#define READ_CONSTANT(function, bytecode) (function->chunk->constants.values[bytecode->as.u8])
#define GET_NODES_ALLOCATOR(inserter) (&(inserter)->nodes_allocator->lox_allocator)

struct pending_evaluate {
    struct bytecode_list * pending_bytecode;
    struct lox_ir_control_node * prev_control_node;
    struct lox_ir_block * block;
};

struct block_local_usage {
    struct u8_set used;
    struct u8_set assigned;
};

struct no_phis_inserter {
    struct block_local_usage current_block_local_usage;
    struct u64_hash_table control_nodes_by_bytecode;
    struct u64_hash_table blocks_by_first_bytecode;
    struct stack_list pending_evaluation;
    struct stack_list data_nodes_stack;
    struct stack_list package_stack;
    long ptions;

    struct lox_ir * lox_ir;
};

static void push_pending_evaluate(
        struct no_phis_inserter *, struct bytecode_list *, struct lox_ir_control_node *, struct lox_ir_block *
);
static struct bytecode_list * simplify_redundant_unconditional_jump_bytecodes(struct bytecode_list *bytecode_list);
static void map_data_nodes_bytecodes_to_control(
        struct u64_hash_table * nodes_by_bytecode,
        struct lox_ir_data_node * data_node,
        struct lox_ir_control_node * to_map_control
);
static void calculate_and_put_use_before_assigment(struct lox_ir_block *, struct no_phis_inserter *);
static struct lox_ir_block * get_block_by_first_bytecode(struct no_phis_inserter *, struct bytecode_list *);
static struct no_phis_inserter * alloc_no_phis_inserter(struct lox_ir *, long);
static void free_no_phis_inserter(struct no_phis_inserter *inserter);
static void jump_if_false(struct no_phis_inserter *, struct pending_evaluate *);
static void jump(struct no_phis_inserter *, struct pending_evaluate *);
static void loop(struct no_phis_inserter *, struct pending_evaluate *);
static void pop(struct no_phis_inserter *, struct pending_evaluate *);
static void set_array_element(struct no_phis_inserter *, struct pending_evaluate *);
static void set_struct_field(struct no_phis_inserter *, struct pending_evaluate *);
static void set_global(struct no_phis_inserter *, struct pending_evaluate *);
static void exit_monitor_explicit(struct no_phis_inserter *, struct pending_evaluate *);
static void exit_monitor_opcode(struct no_phis_inserter *, struct pending_evaluate *);
static void enter_monitor_explicit(struct no_phis_inserter *, struct pending_evaluate *);
static void enter_monitor_opcode(struct no_phis_inserter *, struct pending_evaluate *);
static void return_opcode(struct no_phis_inserter *, struct pending_evaluate *);
static void print(struct no_phis_inserter *, struct pending_evaluate *);
static void set_local(struct no_phis_inserter *, struct pending_evaluate *);
static void get_local(struct no_phis_inserter *, struct pending_evaluate *);
static void call(struct no_phis_inserter *, struct pending_evaluate *);
static void get_global(struct no_phis_inserter *, struct pending_evaluate *);
static void initialize_array(struct no_phis_inserter *, struct pending_evaluate *);
static void initialize_struct(struct no_phis_inserter *, struct pending_evaluate *);
static void get_struct_field(struct no_phis_inserter *, struct pending_evaluate *);
static void get_array_element(struct no_phis_inserter *, struct pending_evaluate *);
static void unary(struct no_phis_inserter *, struct pending_evaluate *);
static void binary(struct no_phis_inserter *, struct pending_evaluate *);
static void add_arguments_guard(struct no_phis_inserter *inserter, struct lox_ir_block *block);
static bool can_optimize_branch(struct no_phis_inserter *, struct bytecode_list *);
static bool can_discard_true_branch(struct no_phis_inserter *, struct bytecode_list *);
static struct instruction_profile_data get_instruction_profile_data(struct no_phis_inserter *, struct bytecode_list *);
static struct object * get_function(struct no_phis_inserter *, struct lox_ir_data_node * function_data_node);
static void add_argument_guard(struct no_phis_inserter*, struct lox_ir_block*, struct type_profile_data, int);
static struct lox_ir_data_guard_node * create_guard(struct no_phis_inserter *, struct lox_ir_data_node * source, struct bytecode_list *);
static bool is_emty_return(struct lox_ir_data_node *);

void create_lox_ir_no_phis(
        struct lox_ir * lox_ir,
        struct bytecode_list * start_function_bytecode,
        long options
) {
    struct no_phis_inserter * inserter = alloc_no_phis_inserter(lox_ir, options);

    push_stack_list(&inserter->package_stack, lox_ir->package);

    struct lox_ir_block * first_block = get_block_by_first_bytecode(inserter, start_function_bytecode);
    push_pending_evaluate(inserter, start_function_bytecode, NULL, first_block);
    lox_ir->first_block = first_block;

    add_arguments_guard(inserter, first_block);

    while (!is_empty_stack_list(&inserter->pending_evaluation)) {
        struct pending_evaluate * to_evaluate = pop_stack_list(&inserter->pending_evaluation);

        if(contains_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode)) {
            //Link two blocks
            struct lox_ir_block * block_evaluated = get_u64_hash_table(&inserter->blocks_by_first_bytecode, (uint64_t) to_evaluate->pending_bytecode);
            struct lox_ir_block * current_block = to_evaluate->block;
            if(block_evaluated != current_block) {
                add_u64_set(&block_evaluated->predecesors, (uint64_t) current_block);
                current_block->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
                current_block->next_as.next = block_evaluated;
            }

            continue;
        }

        switch (to_evaluate->pending_bytecode->bytecode) {
            case OP_JUMP_IF_FALSE: jump_if_false(inserter, to_evaluate); break;
            case OP_JUMP: jump(inserter, to_evaluate); break;
            case OP_LOOP: loop(inserter, to_evaluate); break;
            case OP_POP: pop(inserter, to_evaluate); break;
            case OP_SET_ARRAY_ELEMENT: set_array_element(inserter, to_evaluate); break;
            case OP_SET_STRUCT_FIELD: set_struct_field(inserter, to_evaluate); break;
            case OP_SET_GLOBAL: set_global(inserter, to_evaluate); break;
            case OP_EXIT_MONITOR_EXPLICIT: exit_monitor_explicit(inserter, to_evaluate); break;
            case OP_EXIT_MONITOR: exit_monitor_opcode(inserter, to_evaluate); break;
            case OP_ENTER_MONITOR_EXPLICIT: enter_monitor_explicit(inserter, to_evaluate); break;
            case OP_ENTER_MONITOR: enter_monitor_opcode(inserter, to_evaluate); break;
            case OP_RETURN: return_opcode(inserter, to_evaluate); break;
            case OP_PRINT: print(inserter, to_evaluate); break;
            case OP_SET_LOCAL: set_local(inserter, to_evaluate); break;

            //Expressions -> control nodes
            case OP_GET_LOCAL: get_local(inserter, to_evaluate); break;
            case OP_CALL: call(inserter, to_evaluate); break;
            case OP_GET_GLOBAL: get_global(inserter, to_evaluate); break;
            case OP_INITIALIZE_ARRAY: initialize_array(inserter, to_evaluate); break;
            case OP_INITIALIZE_STRUCT: initialize_struct(inserter, to_evaluate); break;
            case OP_GET_STRUCT_FIELD: get_struct_field(inserter, to_evaluate); break;
            case OP_GET_ARRAY_ELEMENT: get_array_element(inserter, to_evaluate); break;
            case OP_NEGATE:
            case OP_NOT: unary(inserter, to_evaluate); break;
            //Binary operations
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_GREATER:
            case OP_BINARY_OP_AND:
            case OP_BINARY_OP_OR:
            case OP_LEFT_SHIFT:
            case OP_RIGHT_SHIFT:
            case OP_LESS:
            case OP_MODULO:
            case OP_EQUAL:  {
                binary(inserter, to_evaluate);
                break;
            }
            case OP_GET_ARRAY_LENGTH: {
                struct lox_ir_data_get_array_length * get_arr_length = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GET_ARRAY_LENGTH,
                        struct lox_ir_data_get_array_length, to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(lox_ir));
                get_arr_length->instance = pop_stack_list(&inserter->data_nodes_stack);

                push_stack_list(&inserter->data_nodes_stack, get_arr_length);
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
                break;
            }
            case OP_CONSTANT: {
                lox_value_t constant = READ_CONSTANT(lox_ir->function, to_evaluate->pending_bytecode);
                push_stack_list(&inserter->data_nodes_stack, create_lox_ir_const_node(constant, LOX_IR_TYPE_F64, to_evaluate->pending_bytecode,
                        LOX_IR_ALLOCATOR(lox_ir)));
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
                break;
            }
            case OP_FALSE: {
                push_stack_list(&inserter->data_nodes_stack, create_lox_ir_const_node(FALSE_VALUE, LOX_IR_TYPE_LOX_BOOLEAN,
                    to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(lox_ir)));
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
                break;
            }
            case OP_TRUE: {
                push_stack_list(&inserter->data_nodes_stack, create_lox_ir_const_node(TRUE_VALUE, LOX_IR_TYPE_LOX_BOOLEAN,
                    to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(lox_ir)));
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
                break;
            }
            case OP_NIL: {
                push_stack_list(&inserter->data_nodes_stack, create_lox_ir_const_node(NIL_VALUE, LOX_IR_TYPE_LOX_NIL,
                    to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(lox_ir)));
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
                break;
            }
            case OP_FAST_CONST_8: {
                lox_value_t constant = AS_NUMBER(to_evaluate->pending_bytecode->as.u8);
                push_stack_list(&inserter->data_nodes_stack, create_lox_ir_const_node(constant, LOX_IR_TYPE_F64, to_evaluate->pending_bytecode,
                        LOX_IR_ALLOCATOR(lox_ir)));
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
                break;
            }
            case OP_FAST_CONST_16: {
                lox_value_t constant = AS_NUMBER(to_evaluate->pending_bytecode->as.u16);
                push_stack_list(&inserter->data_nodes_stack, create_lox_ir_const_node(constant, LOX_IR_TYPE_F64, to_evaluate->pending_bytecode,
                        LOX_IR_ALLOCATOR(lox_ir)));
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
                break;
            }
            case OP_CONST_1: {
                push_stack_list(&inserter->data_nodes_stack, create_lox_ir_const_node(1, LOX_IR_TYPE_F64, to_evaluate->pending_bytecode,
                        LOX_IR_ALLOCATOR(lox_ir)));
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
                break;
            }
            case OP_CONST_2: {
                push_stack_list(&inserter->data_nodes_stack, create_lox_ir_const_node(2, LOX_IR_TYPE_F64, to_evaluate->pending_bytecode,
                        LOX_IR_ALLOCATOR(lox_ir)));
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
                break;
            }
            case OP_PACKAGE_CONST: {
                lox_value_t package_lox_value = READ_CONSTANT(lox_ir->function, to_evaluate->pending_bytecode);
                struct package_object * package_const = (struct package_object *) AS_OBJECT(package_lox_value);

                push_stack_list(&inserter->package_stack, package_const->package);
                push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
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

        NATIVE_LOX_FREE(to_evaluate);
    }

    free_no_phis_inserter(inserter);
}

static void binary(struct no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct lox_ir_data_binary_node * binaryt_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_BINARY, struct lox_ir_data_binary_node,
            to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(inserter->lox_ir));
    struct lox_ir_data_node * right = pop_stack_list(&inserter->data_nodes_stack);
    struct lox_ir_data_node * left = pop_stack_list(&inserter->data_nodes_stack);
    binaryt_node->operator = to_evaluate->pending_bytecode->bytecode;
    binaryt_node->right = right;
    binaryt_node->left = left;

    push_stack_list(&inserter->data_nodes_stack, binaryt_node);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
}

static void unary(struct no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct lox_ir_data_unary_node * unary_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_UNARY, struct lox_ir_data_unary_node,
            to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(inserter->lox_ir));
    unary_node->operator = to_evaluate->pending_bytecode->bytecode == OP_NEGATE ? UNARY_OPERATION_TYPE_NEGATION : UNARY_OPERATION_TYPE_NOT;
    unary_node->operand = pop_stack_list(&inserter->data_nodes_stack);

    push_stack_list(&inserter->data_nodes_stack, unary_node);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
}

static void get_array_element(struct no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct lox_ir_data_get_array_element_node * get_array_element_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT,
            struct lox_ir_data_get_array_element_node, to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(inserter->lox_ir));
    get_array_element_node->instance = pop_stack_list(&inserter->data_nodes_stack);
    get_array_element_node->index = pop_stack_list(&inserter->data_nodes_stack);

    push_stack_list(&inserter->data_nodes_stack, get_array_element_node);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
}

static void get_struct_field(
        struct no_phis_inserter * inserter,
        struct pending_evaluate * to_evaluate
) {
    struct lox_ir_data_get_struct_field_node * get_struct_field_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GET_STRUCT_FIELD,
            struct lox_ir_data_get_struct_field_node, to_evaluate->pending_bytecode,LOX_IR_ALLOCATOR(inserter->lox_ir));
    get_struct_field_node->instance = pop_stack_list(&inserter->data_nodes_stack);
    get_struct_field_node->field_name = AS_STRING_OBJECT(READ_CONSTANT(inserter->lox_ir->function, to_evaluate->pending_bytecode));

    push_stack_list(&inserter->data_nodes_stack, get_struct_field_node);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
}

static void initialize_array(struct no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct lox_ir_data_initialize_array_node * initialize_array_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_INITIALIZE_ARRAY,
            struct lox_ir_data_initialize_array_node, to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(inserter->lox_ir));
    bool empty_initialization = to_evaluate->pending_bytecode->as.initialize_array.is_emtpy_initializaion;
    uint16_t n_elements = to_evaluate->pending_bytecode->as.initialize_array.n_elements;

    initialize_array_node->empty_initialization = empty_initialization;
    initialize_array_node->n_elements = n_elements;
    if (!empty_initialization) {
        initialize_array_node->elememnts = LOX_MALLOC(LOX_IR_ALLOCATOR(inserter->lox_ir), sizeof(struct lox_ir_data_node *) * n_elements);
    }
    for(int i = n_elements - 1; i >= 0 && !empty_initialization; i--){
        initialize_array_node->elememnts[i] = pop_stack_list(&inserter->data_nodes_stack);
    }

    push_stack_list(&inserter->data_nodes_stack, initialize_array_node);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
}

static void initialize_struct(
        struct no_phis_inserter * inserter,
        struct pending_evaluate * to_evaluate
) {
    struct lox_ir_data_initialize_struct_node * initialize_struct_node = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_INITIALIZE_STRUCT, struct lox_ir_data_initialize_struct_node, to_evaluate->pending_bytecode,
            LOX_IR_ALLOCATOR(inserter->lox_ir));

    struct function_object * function = inserter->lox_ir->function;
    struct struct_definition_object * definition = (struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(function, to_evaluate->pending_bytecode));
    initialize_struct_node->fields_nodes = LOX_MALLOC(LOX_IR_ALLOCATOR(inserter->lox_ir), sizeof(struct struct_definition_object *) * definition->n_fields);
    initialize_struct_node->definition = definition;
    for (int i = definition->n_fields - 1; i >= 0; i--) {
        initialize_struct_node->fields_nodes[i] = pop_stack_list(&inserter->data_nodes_stack);
    }

    push_stack_list(&inserter->data_nodes_stack, initialize_struct_node);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
}

static void get_global(
        struct no_phis_inserter * inserter,
        struct pending_evaluate * to_evaluate
) {
    struct package * global_variable_package = peek_stack_list(&inserter->package_stack);
    struct function_object * function = inserter->lox_ir->function;
    struct string_object * global_variable_name = AS_STRING_OBJECT(READ_CONSTANT(function, to_evaluate->pending_bytecode));
    //If the global variable is constant, we will return a CONST_NODE instead of GET_GLOBAL control_node_to_lower
    if(contains_trie(&global_variable_package->const_global_variables_names, global_variable_name->chars, global_variable_name->length)) {
        lox_value_t constant_value = get_hash_table(&global_variable_package->global_variables, global_variable_name);
        struct lox_ir_data_constant_node * constant_node = create_lox_ir_const_node(constant_value, LOX_IR_TYPE_LOX_ANY,
                to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(inserter->lox_ir));
        push_stack_list(&inserter->data_nodes_stack, constant_node);

    } else { //Non constant global variable
        struct lox_ir_data_get_global_node * get_global_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GET_GLOBAL,
                struct lox_ir_data_get_global_node, to_evaluate->pending_bytecode,LOX_IR_ALLOCATOR(inserter->lox_ir));
        get_global_node->package = global_variable_package;
        get_global_node->name = global_variable_name;

        push_stack_list(&inserter->data_nodes_stack, get_global_node);
    }

    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);

}

static void call(struct no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct lox_ir_data_function_call_node * call_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_CALL, struct lox_ir_data_function_call_node,
            to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(inserter->lox_ir));
    bool is_paralell = to_evaluate->pending_bytecode->as.pair.u8_2;
    uint8_t n_args = to_evaluate->pending_bytecode->as.pair.u8_1;
    struct lox_ir_data_node * function_to_call_data_node = peek_n_stack_list(&inserter->data_nodes_stack, n_args);
    struct object * function_to_call_object = get_function(inserter, function_to_call_data_node);

    if (function_to_call_object->type == OBJ_NATIVE_FUNCTION) {
        call_node->native_function = (struct native_function_object *) function_to_call_data_node;
        call_node->is_native = true;
    } else {
        call_node->lox_function.function = (struct function_object *) function_to_call_data_node;
        call_node->lox_function.is_parallel = is_paralell;
        call_node->is_native = false;
    }

    call_node->n_arguments = n_args;
    call_node->arguments = LOX_MALLOC(LOX_IR_ALLOCATOR(inserter->lox_ir), sizeof(struct lox_ir_data_node *) * n_args);
    for (int i = n_args; i > 0; i--) {
        struct lox_ir_data_node * argument = pop_stack_list(&inserter->data_nodes_stack);
        call_node->arguments[i - 1] = argument;
    }

    //Remove function_to_call_object object in the stack
    pop_stack_list(&inserter->data_nodes_stack);

    //Add guard, if the returned value type has a profiled data
    struct lox_ir_data_guard_node * guard_node = create_guard(inserter, &call_node->data, to_evaluate->pending_bytecode);
    if(guard_node != NULL){
        push_stack_list(&inserter->data_nodes_stack, &guard_node->data);
    } else {
        push_stack_list(&inserter->data_nodes_stack, &call_node->data);
    }

    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
}

static void get_local(
        struct no_phis_inserter * inserter,
        struct pending_evaluate * to_evaluate
) {
    struct lox_ir_data_get_local_node * get_local_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GET_LOCAL, struct lox_ir_data_get_local_node,
            to_evaluate->pending_bytecode, LOX_IR_ALLOCATOR(inserter->lox_ir));

    //Pending initialize
    int local_number = to_evaluate->pending_bytecode->as.u8;
    get_local_node->local_number = local_number;

    add_u8_set(&inserter->current_block_local_usage.used, local_number);

    push_stack_list(&inserter->data_nodes_stack, get_local_node);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, to_evaluate->prev_control_node, to_evaluate->block);
}

static void set_local(struct no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct lox_ir_control_set_local_node * set_local_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTORL_NODE_SET_LOCAL, struct lox_ir_control_set_local_node,
            to_evaluate->block, LOX_IR_ALLOCATOR(inserter->lox_ir));
    struct lox_ir_data_node * new_local_value = pop_stack_list(&inserter->data_nodes_stack);
    int local_number = to_evaluate->pending_bytecode->as.u8;
    set_local_node->new_local_value = new_local_value;
    set_local_node->local_number = local_number;
    add_u8_set(&inserter->current_block_local_usage.assigned, local_number);

    calculate_and_put_use_before_assigment(to_evaluate->block, inserter);
    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, new_local_value, &set_local_node->control);

    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evaluate->block, &set_local_node->control);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, &set_local_node->control, to_evaluate->block);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode, set_local_node);
}

static void print(struct no_phis_inserter * inserter, struct pending_evaluate * to_evaluate) {
    struct lox_ir_control_print_node * print_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_PRINT, struct lox_ir_control_print_node,
            to_evaluate->block, LOX_IR_ALLOCATOR(inserter->lox_ir));
    struct lox_ir_data_node * print_value = pop_stack_list(&inserter->data_nodes_stack);
    print_node->data = print_value;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, print_value, &print_node->control);
    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evaluate->block, &print_node->control);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, &print_node->control, to_evaluate->block);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode, print_node);
}

static void return_opcode(struct no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct lox_ir_control_return_node * return_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_RETURN, struct lox_ir_control_return_node,
            to_evalute->block, LOX_IR_ALLOCATOR(inserter->lox_ir));
    struct lox_ir_data_node * return_value = pop_stack_list(&inserter->data_nodes_stack);
    return_node->empty_return = is_emty_return(return_value);
    return_node->data = return_value;

    to_evalute->block->type_next = TYPE_NEXT_LOX_IR_BLOCK_NONE;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, return_value, &return_node->control);
    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evalute->block, &return_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, return_node);
    add_u64_set(&inserter->lox_ir->exit_blocks, (uint64_t) to_evalute->block);
}

static void enter_monitor_opcode(
        struct no_phis_inserter * inserter,
        struct pending_evaluate * to_evaluate
) {
    monitor_number_t monitor_number = to_evaluate->pending_bytecode->as.u8;
    struct function_object * function = inserter->lox_ir->function;
    struct monitor * monitor = &function->monitors[monitor_number];
    struct lox_ir_control_enter_monitor_node * enter_monitor_node = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_ENTER_MONITOR, struct lox_ir_control_enter_monitor_node, to_evaluate->block,
            LOX_IR_ALLOCATOR(inserter->lox_ir)
    );

    enter_monitor_node->monitor_number = monitor_number;
    enter_monitor_node->monitor = monitor;

    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evaluate->block, &enter_monitor_node->control);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, &enter_monitor_node->control, to_evaluate->block);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode, enter_monitor_node);
}

static void enter_monitor_explicit(struct no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct monitor * monitor = (struct monitor *) to_evalute->pending_bytecode->as.u64;
    struct lox_ir_control_enter_monitor_node * enter_monitor_node = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_ENTER_MONITOR, struct lox_ir_control_enter_monitor_node, to_evalute->block,
                    LOX_IR_ALLOCATOR(inserter->lox_ir)
    );

    struct function_object * function = inserter->lox_ir->function;
    enter_monitor_node->monitor_number = (monitor_number_t) (monitor - &function->monitors[0]);
    enter_monitor_node->monitor = monitor;

    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evalute->block, &enter_monitor_node->control);
    push_pending_evaluate(inserter, to_evalute->pending_bytecode->next, &enter_monitor_node->control, to_evalute->block);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, enter_monitor_node);
}

static void exit_monitor_opcode(
        struct no_phis_inserter * inserter,
        struct pending_evaluate * to_evalute
) {
    monitor_number_t monitor_number = to_evalute->pending_bytecode->as.u8;
    struct monitor * monitor = &inserter->lox_ir->function->monitors[monitor_number];
    struct lox_ir_control_exit_monitor_node * exit_monitor_node = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_EXIT_MONITOR, struct lox_ir_control_exit_monitor_node, to_evalute->block,
            LOX_IR_ALLOCATOR(inserter->lox_ir)
    );

    exit_monitor_node->monitor_number = monitor_number;
    exit_monitor_node->monitor = monitor;

    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evalute->block, &exit_monitor_node->control);
    push_pending_evaluate(inserter, to_evalute->pending_bytecode->next, &exit_monitor_node->control, to_evalute->block);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, exit_monitor_node);
}

static void exit_monitor_explicit(struct no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct monitor * monitor = (struct monitor *) to_evalute->pending_bytecode->as.u64;
    struct lox_ir_control_exit_monitor_node * exit_monitor_node = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_EXIT_MONITOR, struct lox_ir_control_exit_monitor_node, to_evalute->block,
            LOX_IR_ALLOCATOR(inserter->lox_ir)
    );

    struct function_object * function = inserter->lox_ir->function;
    exit_monitor_node->monitor_number = (monitor_number_t) (monitor - &function->monitors[0]);
    exit_monitor_node->monitor = monitor;

    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evalute->block, &exit_monitor_node->control);
    push_pending_evaluate(inserter, to_evalute->pending_bytecode->next, &exit_monitor_node->control, to_evalute->block);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, exit_monitor_node);
}

static void set_global(
        struct no_phis_inserter * inserter,
        struct pending_evaluate * to_evaluate
) {
    struct lox_ir_control_set_global_node * set_global_node = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTORL_NODE_SET_GLOBAL, struct lox_ir_control_set_global_node, to_evaluate->block,
            LOX_IR_ALLOCATOR(inserter->lox_ir)
    );

    set_global_node->name = AS_STRING_OBJECT(READ_CONSTANT(inserter->lox_ir->function, to_evaluate->pending_bytecode));
    set_global_node->value_node = pop_stack_list(&inserter->data_nodes_stack);
    set_global_node->package = peek_stack_list(&inserter->package_stack);

    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evaluate->block, &set_global_node->control);
    push_pending_evaluate(inserter, to_evaluate->pending_bytecode->next, &set_global_node->control, to_evaluate->block);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evaluate->pending_bytecode, set_global_node);
}

static void set_struct_field(
        struct no_phis_inserter * inserter,
        struct pending_evaluate * to_evalute
) {
    struct lox_ir_control_set_struct_field_node * set_struct_field = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD, struct lox_ir_control_set_struct_field_node, to_evalute->block,
                    LOX_IR_ALLOCATOR(inserter->lox_ir)
    );
    struct lox_ir_data_node * field_value = pop_stack_list(&inserter->data_nodes_stack);
    struct lox_ir_data_node * instance = pop_stack_list(&inserter->data_nodes_stack);

    set_struct_field->field_name = AS_STRING_OBJECT(READ_CONSTANT(inserter->lox_ir->function, to_evalute->pending_bytecode));
    set_struct_field->new_field_value = field_value;
    set_struct_field->instance = instance;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, field_value, &set_struct_field->control);
    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, instance, &set_struct_field->control);
    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evalute->block, &set_struct_field->control);
    push_pending_evaluate(inserter, to_evalute->pending_bytecode->next, &set_struct_field->control, to_evalute->block);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, set_struct_field);
}

static void set_array_element(struct no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct lox_ir_control_set_array_element_node * set_arrary_element_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT,
            struct lox_ir_control_set_array_element_node, to_evalute->block, LOX_IR_ALLOCATOR(inserter->lox_ir));
    struct lox_ir_data_node * instance = pop_stack_list(&inserter->data_nodes_stack);
    struct lox_ir_data_node * new_element = pop_stack_list(&inserter->data_nodes_stack);
    struct lox_ir_data_node * index = pop_stack_list(&inserter->data_nodes_stack);

    set_arrary_element_node->index = index;
    set_arrary_element_node->new_element_value = new_element;
    set_arrary_element_node->array = instance;

    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, instance, &set_arrary_element_node->control);
    map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, new_element, &set_arrary_element_node->control);
    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evalute->block, &set_arrary_element_node->control);
    push_pending_evaluate(inserter, to_evalute->pending_bytecode->next, &set_arrary_element_node->control, to_evalute->block);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, set_arrary_element_node);
}

//Expression statements
static void pop(struct no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    if(!is_empty_stack_list(&inserter->data_nodes_stack)){
        struct lox_ir_data_node * data_node = pop_stack_list(&inserter->data_nodes_stack);
        struct lox_ir_control_data_node * control_data_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_DATA, struct lox_ir_control_data_node,
                to_evalute->block, LOX_IR_ALLOCATOR(inserter->lox_ir));

        control_data_node->data = data_node;

        map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, data_node, &control_data_node->control);
        add_last_control_node_block_lox_ir(inserter->lox_ir, to_evalute->block, &control_data_node->control);
        push_pending_evaluate(inserter, to_evalute->pending_bytecode->next, &control_data_node->control, to_evalute->block);
        put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, control_data_node);
    } else {
        push_pending_evaluate(inserter, to_evalute->pending_bytecode->next, to_evalute->prev_control_node, to_evalute->block);
    }
}

//Loop body is not added to the predecesors set of loop jump_to_operand
static void loop(struct no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct lox_ir_control_loop_jump_node * loop_jump_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_LOOP_JUMP, struct lox_ir_control_loop_jump_node,
            to_evalute->block, LOX_IR_ALLOCATOR(inserter->lox_ir));
    struct bytecode_list * to_jump_bytecode = to_evalute->pending_bytecode->as.jump;
    struct bytecode_list * loop_condition_bytecode = get_next_bytecode_list(to_jump_bytecode, OP_JUMP_IF_FALSE);
    struct lox_ir_control_node * to_jump_node = get_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_jump_bytecode);

    to_evalute->block->type_next = TYPE_NEXT_LOX_IR_BLOCK_LOOP;
    to_evalute->block->next_as.loop = get_u64_hash_table(&inserter->blocks_by_first_bytecode, (uint64_t) loop_condition_bytecode);

    clear_u8_set(&inserter->current_block_local_usage.assigned);
    clear_u8_set(&inserter->current_block_local_usage.used);
    add_last_control_node_block_lox_ir(inserter->lox_ir, to_evalute->block, &loop_jump_node->control);
    put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, loop_jump_node);
}

static void jump(struct no_phis_inserter * insterter, struct pending_evaluate * to_evalute) {
    //OP_JUMP are translated to edges in the lox ir graph
    struct bytecode_list * to_jump_bytecode = simplify_redundant_unconditional_jump_bytecodes(to_evalute->pending_bytecode->as.jump);
    struct lox_ir_block * new_block = get_block_by_first_bytecode(insterter, to_jump_bytecode);

    new_block->loop_condition_block = to_evalute->block->loop_condition_block;
    new_block->nested_loop_body = to_evalute->block->nested_loop_body;

    to_evalute->block->last = to_evalute->prev_control_node;
    to_evalute->block->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
    to_evalute->block->next_as.next = new_block;

    add_u64_set(&new_block->predecesors, (uint64_t) to_evalute->block);

    clear_u8_set(&insterter->current_block_local_usage.assigned);
    clear_u8_set(&insterter->current_block_local_usage.used);
    push_pending_evaluate(insterter, to_jump_bytecode, NULL, new_block);
}

static void jump_if_false(struct no_phis_inserter * inserter, struct pending_evaluate * to_evalute) {
    struct lox_ir_data_node * condition = pop_stack_list(&inserter->data_nodes_stack);

    struct bytecode_list * false_branch_bytecode = simplify_redundant_unconditional_jump_bytecodes(to_evalute->pending_bytecode->as.jump);
    struct bytecode_list * true_branch_bytecode = simplify_redundant_unconditional_jump_bytecodes(to_evalute->pending_bytecode->next);
    struct lox_ir_block * parent_block = to_evalute->block;
    struct lox_ir_block * true_branch_block = get_block_by_first_bytecode(inserter, true_branch_bytecode);
    struct lox_ir_block * false_branch_block = get_block_by_first_bytecode(inserter, false_branch_bytecode);

    //Loop conditions, are creatd in the graph as a separated block
    if(to_evalute->pending_bytecode->loop_condition) {
        struct lox_ir_control_conditional_jump_node * cond_jump_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP,
                struct lox_ir_control_conditional_jump_node, to_evalute->block, LOX_IR_ALLOCATOR(inserter->lox_ir));

        struct lox_ir_block  *condition_block = get_block_by_first_bytecode(inserter, to_evalute->pending_bytecode);
        parent_block->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
        parent_block->next_as.next = condition_block;

        condition_block->type_next = TYPE_NEXT_LOX_IR_BLOCK_BRANCH;
        condition_block->next_as.branch.true_branch = true_branch_block;
        condition_block->next_as.branch.false_branch = false_branch_block;
        condition_block->is_loop_condition = true;
        condition_block->nested_loop_body = parent_block->nested_loop_body + 1;
        condition_block->loop_condition_block = parent_block->loop_condition_block;
        cond_jump_node->control.block = condition_block;
        cond_jump_node->condition = condition;

        true_branch_block->nested_loop_body = parent_block->nested_loop_body + 1;
        false_branch_block->nested_loop_body = parent_block->nested_loop_body;
        true_branch_block->loop_condition_block = condition_block;
        false_branch_block->loop_condition_block = parent_block->loop_condition_block;

        add_u64_set(&condition_block->predecesors, (uint64_t) parent_block);
        add_u64_set(&false_branch_block->predecesors, (uint64_t) condition_block);
        add_u64_set(&true_branch_block->predecesors, (uint64_t) condition_block);

        add_last_control_node_block_lox_ir(inserter->lox_ir, condition_block, &cond_jump_node->control);

        push_pending_evaluate(inserter, false_branch_bytecode, NULL, false_branch_block);
        push_pending_evaluate(inserter, true_branch_bytecode, NULL, true_branch_block);
        clear_u8_set(&inserter->current_block_local_usage.assigned);
        clear_u8_set(&inserter->current_block_local_usage.used);
        put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, cond_jump_node);
        map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, condition, &cond_jump_node->control);

    } else if(IS_FLAG_SET(inserter->ptions, LOX_IR_CREATION_OPT_USE_BRANCH_PROFILE) &&
              can_optimize_branch(inserter, to_evalute->pending_bytecode)) {

        struct lox_ir_control_guard_node * guard_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_GUARD, struct lox_ir_control_guard_node,
                to_evalute->block, LOX_IR_ALLOCATOR(inserter->lox_ir));
        guard_node->guard.value_to_compare.check_true = !can_discard_true_branch(inserter, to_evalute->pending_bytecode);
        guard_node->guard.action_on_guard_failed = LOX_IR_GUARD_FAIL_ACTION_TYPE_SWITCH_TO_INTERPRETER;
        guard_node->guard.type = LOX_IR_GUARD_BOOLEAN_CHECK;
        guard_node->guard.value = condition;

        struct bytecode_list * remaining_bytecode = can_discard_true_branch(inserter, to_evalute->pending_bytecode) ?
                false_branch_bytecode : true_branch_bytecode;

        add_last_control_node_block_lox_ir(inserter->lox_ir, parent_block, &guard_node->control);
        push_pending_evaluate(inserter, remaining_bytecode, NULL, parent_block);

    } else {
        struct lox_ir_control_conditional_jump_node * cond_jump_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP,
                struct lox_ir_control_conditional_jump_node, to_evalute->block, LOX_IR_ALLOCATOR(inserter->lox_ir));
        cond_jump_node->condition = condition;

        parent_block->type_next = TYPE_NEXT_LOX_IR_BLOCK_BRANCH;
        parent_block->next_as.branch.false_branch = false_branch_block;
        parent_block->next_as.branch.true_branch = true_branch_block;

        false_branch_block->nested_loop_body = parent_block->nested_loop_body;
        true_branch_block->nested_loop_body = parent_block->nested_loop_body;
        false_branch_block->loop_condition_block = parent_block->loop_condition_block;
        true_branch_block->loop_condition_block = parent_block->loop_condition_block;

        add_u64_set(&false_branch_block->predecesors, (uint64_t) parent_block);
        add_u64_set(&true_branch_block->predecesors, (uint64_t) parent_block);

        add_last_control_node_block_lox_ir(inserter->lox_ir, parent_block, &cond_jump_node->control);

        push_pending_evaluate(inserter, false_branch_bytecode, NULL, false_branch_block);
        push_pending_evaluate(inserter, true_branch_bytecode, NULL, true_branch_block);
        clear_u8_set(&inserter->current_block_local_usage.assigned);
        clear_u8_set(&inserter->current_block_local_usage.used);
        put_u64_hash_table(&inserter->control_nodes_by_bytecode, (uint64_t) to_evalute->pending_bytecode, cond_jump_node);
        map_data_nodes_bytecodes_to_control(&inserter->control_nodes_by_bytecode, condition, &cond_jump_node->control);
    }
}

static void push_pending_evaluate(
        struct no_phis_inserter * inserter,
        struct bytecode_list * pending_bytecode,
        struct lox_ir_control_node * prev_lox_ir_node,
        struct lox_ir_block * block
) {
    if (pending_bytecode != NULL) {
        struct pending_evaluate * pending_evalutaion = NATIVE_LOX_MALLOC(sizeof(struct pending_evaluate));
        pending_evalutaion->prev_control_node = prev_lox_ir_node;
        pending_evalutaion->pending_bytecode = pending_bytecode;
        pending_evalutaion->block = block;
        push_stack_list(&inserter->pending_evaluation, pending_evalutaion);
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
    struct u64_hash_table * control_nodes_by_bytecode;
    struct lox_ir_control_node * to_map_control;
};

static bool map_data_nodes_bytecodes_to_control_consumer(
        struct lox_ir_data_node * _,
        void ** __,
        struct lox_ir_data_node * current_node,
        void * extra
) {
    struct map_data_nodes_bytecodes_to_control_struct * consumer_struct = extra;

    //Add data_node
    put_u64_hash_table(consumer_struct->control_nodes_by_bytecode, (uint64_t) current_node->original_bytecode,
                       consumer_struct->to_map_control);

    return true;
}

//This function will map the control control_node_to_lower bytecode to the to_map_control control control_node_to_lower
//Example: Given OP_CONST_1, OP_CONST_2, OP_ADD, OP_PRINT
//The first 3 bytecodes will point to OP_PRINT
static void map_data_nodes_bytecodes_to_control(
        struct u64_hash_table * nodes_by_bytecode,
        struct lox_ir_data_node * data_node,
        struct lox_ir_control_node * to_map_control
) {
    struct map_data_nodes_bytecodes_to_control_struct consumer_struct = (struct map_data_nodes_bytecodes_to_control_struct) {
            .control_nodes_by_bytecode = nodes_by_bytecode,
            .to_map_control = to_map_control,
    };

    for_each_lox_ir_data_node(
            data_node,
            NULL,
            &consumer_struct,
            LOX_IR_DATA_NODE_OPT_POST_ORDER,
            map_data_nodes_bytecodes_to_control_consumer
    );
}

//TODO Added local no_phis_inserter arena
static struct no_phis_inserter * alloc_no_phis_inserter(struct lox_ir * lox_ir, long options) {
    struct no_phis_inserter * no_phis_inserter = NATIVE_LOX_MALLOC(sizeof(struct no_phis_inserter));
    init_u8_set(&no_phis_inserter->current_block_local_usage.assigned);
    no_phis_inserter->lox_ir = lox_ir;
    init_u64_hash_table(&no_phis_inserter->control_nodes_by_bytecode, NATIVE_LOX_ALLOCATOR());
    init_u64_hash_table(&no_phis_inserter->blocks_by_first_bytecode, NATIVE_LOX_ALLOCATOR());
    init_u8_set(&no_phis_inserter->current_block_local_usage.used);
    init_stack_list(&no_phis_inserter->pending_evaluation, NATIVE_LOX_ALLOCATOR());
    init_stack_list(&no_phis_inserter->data_nodes_stack, NATIVE_LOX_ALLOCATOR());
    init_stack_list(&no_phis_inserter->package_stack, NATIVE_LOX_ALLOCATOR());
    no_phis_inserter->ptions = options;
    return no_phis_inserter;
}

static void free_no_phis_inserter(struct no_phis_inserter * inserter) {
    free_u64_hash_table(&inserter->control_nodes_by_bytecode);
    free_u64_hash_table(&inserter->blocks_by_first_bytecode);
    free_stack_list(&inserter->pending_evaluation);
    free_stack_list(&inserter->data_nodes_stack);
    free_stack_list(&inserter->package_stack);
}

static struct lox_ir_block * get_block_by_first_bytecode(
        struct no_phis_inserter * inserter,
        struct bytecode_list * first_bytecode
) {
    if(contains_u64_hash_table(&inserter->blocks_by_first_bytecode, (uint64_t) first_bytecode)){
        return get_u64_hash_table(&inserter->blocks_by_first_bytecode, (uint64_t) first_bytecode);
    } else {
        struct lox_ir_block * new_block = alloc_lox_ir_block(LOX_IR_ALLOCATOR(inserter->lox_ir));
        put_u64_hash_table(&inserter->blocks_by_first_bytecode, (uint64_t) first_bytecode, new_block);
        return new_block;
    }
}

static void calculate_and_put_use_before_assigment(struct lox_ir_block * block, struct no_phis_inserter * inserter) {
    struct block_local_usage local_usage = inserter->current_block_local_usage;
    struct u8_set use_before_assigment = local_usage.assigned;

    intersection_u8_set(&use_before_assigment, local_usage.used);
    clear_u8_set(&local_usage.assigned);
    union_u8_set(&block->use_before_assigment, use_before_assigment);
}

static void add_arguments_guard(struct no_phis_inserter * inserter, struct lox_ir_block * block) {
    struct function_object * function = inserter->lox_ir->function;
    struct function_profile_data function_profile = function->state_as.profiling.profile_data;

    for (int i = 0; i < function->n_arguments; i++) {
        struct type_profile_data argument_type_profile_data = function_profile.arguments_type_profile[i];
        add_argument_guard(inserter, block, argument_type_profile_data, i);
    }
}

static void add_argument_guard(
        struct no_phis_inserter * inserter,
        struct lox_ir_block * block,
        struct type_profile_data type_profile,
        int argument_local_number
) {
    profile_data_type_t argument_profiled_type = get_type_by_type_profile_data(type_profile);
    if(argument_profiled_type == PROFILE_DATA_TYPE_ANY) {
        return;
    }

    struct lox_ir_control_guard_node * guard_node = NULL;
    if(argument_profiled_type == PROFILE_DATA_TYPE_STRUCT_INSTANCE && !type_profile.invalid_struct_definition){
        guard_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_GUARD, struct lox_ir_control_guard_node, block,
                LOX_IR_ALLOCATOR(inserter->lox_ir));
        guard_node->guard.type = LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK;
        guard_node->guard.value_to_compare.struct_definition = type_profile.struct_definition;
    } else {
        guard_node = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_GUARD, struct lox_ir_control_guard_node, block,
                LOX_IR_ALLOCATOR(inserter->lox_ir));
        guard_node->guard.value_to_compare.type = profiled_type_to_lox_ir_type(argument_profiled_type);
        guard_node->guard.type = LOX_IR_GUARD_TYPE_CHECK;
    }

    guard_node->guard.action_on_guard_failed = LOX_IR_GUARD_FAIL_ACTION_TYPE_SWITCH_TO_INTERPRETER;

    struct lox_ir_data_get_local_node * get_argument = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GET_LOCAL, struct lox_ir_data_get_local_node,
            NULL, LOX_IR_ALLOCATOR(inserter->lox_ir));
    guard_node->guard.value = &get_argument->data;
    get_argument->local_number = argument_local_number;

    add_last_control_node_block_lox_ir(inserter->lox_ir, block, &guard_node->control);
}

static bool can_optimize_branch(struct no_phis_inserter * inserter, struct bytecode_list * condition_bytecode) {
    struct instruction_profile_data instruction_profile = get_instruction_profile_data(inserter, condition_bytecode);
    return can_false_branch_be_discarded_instruction_profile_data(instruction_profile) ||
        can_true_branch_be_discarded_instruction_profile_data(instruction_profile);
}

static bool can_discard_true_branch(struct no_phis_inserter * inserter, struct bytecode_list * condition_bytecode) {
    struct instruction_profile_data instruction_profile = get_instruction_profile_data(inserter, condition_bytecode);
    return can_true_branch_be_discarded_instruction_profile_data(instruction_profile);
}

static struct instruction_profile_data get_instruction_profile_data(
        struct no_phis_inserter * inserter,
        struct bytecode_list * bytecode
) {
    return get_instruction_profile_data_function(inserter->lox_ir->function, bytecode);
}

static struct object * get_function(struct no_phis_inserter * inserter, struct lox_ir_data_node * function_data_node) {
    lox_assert(function_data_node->type == LOX_IR_DATA_NODE_GET_GLOBAL, "no_phis_creator.c::get_function",
               "Invalid bytecode %i, only functions can be accessed with GET_GLOBAL");

    struct lox_ir_data_get_global_node * get_global = (struct lox_ir_data_get_global_node *) function_data_node;
    lox_value_t function_lox_value = get_hash_table(&get_global->package->global_variables, get_global->name);

    if(!IS_OBJECT(function_lox_value)){
        return NULL;
    }

    return AS_OBJECT(function_lox_value);
}

static struct lox_ir_data_guard_node * create_guard(
        struct no_phis_inserter * inserter,
        struct lox_ir_data_node * source,
        struct bytecode_list * call_bytecode
) {
    struct function_call_profile function_call_profile = get_instruction_profile_data(inserter,
            call_bytecode).as.function_call;
    struct type_profile_data return_type_profile_data = function_call_profile.returned_type_profile;
    profile_data_type_t return_type_profile = get_type_by_type_profile_data(return_type_profile_data);

    if (return_type_profile == PROFILE_DATA_TYPE_ANY ||
        return_type_profile == PROFILE_DATA_TYPE_NIL ||
        call_bytecode->next->bytecode != OP_POP
    ) {
        return NULL;
    }

    return create_from_profile_lox_ir_data_guard_node(return_type_profile_data, source,
        LOX_IR_ALLOCATOR(inserter->lox_ir), LOX_IR_GUARD_FAIL_ACTION_TYPE_SWITCH_TO_INTERPRETER);
}

static bool is_emty_return(struct lox_ir_data_node * return_value) {
    return return_value->type == LOX_IR_DATA_NODE_CONSTANT &&
            IS_NIL_LOX_IR_TYPE(return_value->produced_type->type);
}