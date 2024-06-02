#include "function_inliner.h"

static void rename_local_variables(struct bytecode_list * to_inline, struct function_object * target);
static void remove_double_emtpy_return(struct bytecode_list *to_inline);
static void remove_return_statements(struct bytecode_list * to_inline, struct function_object * target);
static void rename_argument_passing(struct function_object * target_function, struct bytecode_list * target_chunk,
        int call_index, int n_arguments);
static void get_call_args_in_stack(struct stack_list *, struct bytecode_list * target_chunk,
        struct function_object * target_functions, int call_index);
static void merge_to_inline_and_target(struct bytecode_list * merge_node, struct bytecode_list * to_inline);
static void remove_op_call(struct bytecode_list * call_node);

struct function_inline_result inline_function(
        struct function_object * target,
        int chunk_target_index,
        struct function_object * function_to_inline
) {
    struct bytecode_list * chunk_to_inline = create_bytecode_list(function_to_inline->chunk);
    struct bytecode_list * target_chunk = create_bytecode_list(target->chunk);
    struct bytecode_list * target_call = get_by_index_bytecode_list(target_chunk, chunk_target_index);
    int n_arguments_to_inline = function_to_inline->n_arguments;

    rename_local_variables(chunk_to_inline, target);
    remove_double_emtpy_return(chunk_to_inline);
    remove_return_statements(chunk_to_inline, target);

    rename_argument_passing(target, target_chunk, chunk_target_index, n_arguments_to_inline);
    merge_to_inline_and_target(target_call, chunk_to_inline);
    remove_op_call(target_call);

    struct chunk * result_chunk = to_chunk_bytecode_list(target_chunk);

    free_bytecode_list(target_call);

    return (struct function_inline_result) {
        .inlined_chunk = result_chunk,
        .total_size_added = result_chunk->in_use - target->chunk->in_use
    };
}

static void remove_op_call(struct bytecode_list * call_node) {
    unlink_instruciton_bytecode_list(call_node);
    free(call_node);
}

static void merge_to_inline_and_target(struct bytecode_list * merge_node, struct bytecode_list * to_inline) {
    add_instructions_bytecode_list(merge_node, to_inline);
}

static void rename_argument_passing(
        struct function_object * target_function,
        struct bytecode_list * target_chunk,
        int call_index,
        int n_arguments
) {
    if (n_arguments == 0) {
        return;
    }

    struct bytecode_list * call_node = get_by_index_bytecode_list(target_chunk, call_index);

    struct stack_list stack;
    init_stack_list(&stack);
    get_call_args_in_stack(&stack, target_chunk, target_function, call_index);

    for (int i = 0; i < n_arguments; i++) {
        struct bytecode_list * arg_node = pop_stack_list(&stack);
        struct bytecode_list * set_local = alloc_bytecode_list();
        set_local->bytecode = OP_SET_LOCAL;
        set_local->as.u8 = i + target_function->n_locals;

        add_instruction_bytecode_list(arg_node, set_local);
    }

    //Remove OP_GET_GLOBAL <function name>
    struct bytecode_list * get_function_node = pop_stack_list(&stack);
    unlink_instruciton_bytecode_list(get_function_node);
}

static void get_call_args_in_stack(
        struct stack_list * stack,
        struct bytecode_list * target_chunk,
        struct function_object * target_function,
        int call_index
) {
    struct bytecode_list * call_node = get_by_index_bytecode_list(target_chunk, call_index);
    struct bytecode_list * current_node = target_chunk;

    while (current_node < call_node) {
        int current_instruction_n_push = get_n_push_bytecode_instruction(current_node->bytecode);
        int current_instruction_n_pop = get_n_pop_bytecode_instruction(current_node->bytecode);

        if(current_instruction_n_pop > 0 && current_instruction_n_pop != N_VARIABLE_INSTRUCTION_N_POPS){
            pop_n_stack_list(stack, current_instruction_n_pop);
        }
        if(current_instruction_n_pop == N_VARIABLE_INSTRUCTION_N_POPS) {
            switch (current_node->bytecode) {
                case OP_INITIALIZE_STRUCT:
                    uint8_t struct_definition_constant = current_node->as.u8;
                    struct struct_definition_object * struct_definition = (struct struct_definition_object *) AS_OBJECT(
                            target_function->chunk->constants.values[struct_definition_constant]);
                    int n_fields = struct_definition->n_fields;

                    pop_n_stack_list(stack, n_fields);

                    break;
                case OP_INITIALIZE_ARRAY:
                    pop_n_stack_list(stack, current_node->as.u16);
                    break;
                case OP_CALL:
                    pop_n_stack_list(stack, current_node->as.pair.u8_1);
                    break;
                default:
                    break;
            }
        }
        if (current_instruction_n_push > 0) {
            push_n_stack_list(stack, current_node, current_instruction_n_push);
        }

        current_node = current_node->next;
    }
}

static void rename_local_variables(struct bytecode_list * to_inline, struct function_object * target) {
    int to_add_local_variables = target->n_locals;

    for(struct bytecode_list * current_to_inline = to_inline;
        current_to_inline != NULL;
        current_to_inline = current_to_inline->next) {

        if(current_to_inline->bytecode == OP_GET_LOCAL || current_to_inline->bytecode == OP_SET_LOCAL){
            current_to_inline->as.u8 = current_to_inline->as.u8 + to_add_local_variables;
        }
    }
}

//If we had return 1, the compiled bytecode would be "OP_CONST_1 OP_RETURN OP_NIL OP_RETURN" The compiler is inserting
//An empty return so an item will always be placed in the stack if the user didn't put any return.
//We want to get rid of "OP_NIL OP_RETURN" If previusly we have already returned, so OP_NIL OP_RETURN doesn't get inlined
static void remove_double_emtpy_return(struct bytecode_list * to_inline) {
    bytecode_t current_instruction = 0;
    bytecode_t prev_instruction = 0;
    struct bytecode_list * current_node = to_inline;

    while(current_node != NULL){
        current_instruction = current_node->bytecode;

        if(current_instruction == OP_RETURN &&
           prev_instruction == OP_RETURN &&
           current_node->next->bytecode == OP_NIL) {

            struct bytecode_list * next_to_current = current_node->next;

            unlink_instruciton_bytecode_list(current_node);

            current_node = next_to_current;
        } else {
            prev_instruction = current_instruction;
            current_node = current_node->next;
        }
    }
}

static void remove_return_statements(struct bytecode_list * to_inline, struct function_object * target) {
    int return_local_variable_num = target->n_locals;

    for(struct bytecode_list * current_to_inline = to_inline;
        current_to_inline != NULL;
        current_to_inline = current_to_inline->next) {

        if (current_to_inline->bytecode == OP_RETURN) {
            //The callee will always place the return value in the stack
            //The caller will always pop it from the stack or OP_POP instruction will be used
            //So we can discard return statements
            unlink_instruciton_bytecode_list(current_to_inline);
        }
    }
}