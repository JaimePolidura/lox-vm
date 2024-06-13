#include "function_inliner.h"

static void rename_local_variables(struct bytecode_list * to_inline, struct function_object * target);
static void remove_double_emtpy_return(struct bytecode_list *to_inline);
static void rename_return_statements(struct bytecode_list * to_inline, struct function_object * target);
static void rename_argument_passing(struct function_object * target_function, struct bytecode_list ** target_chunk,
        int call_index, int n_arguments);
static void get_call_args_in_stack(struct stack_list *, struct bytecode_list * target_chunk,
        struct function_object * target_functions, int call_index);
static void merge_to_inline_and_target(struct bytecode_list * merge_node, struct bytecode_list * to_inline);
static void remove_op_call(struct bytecode_list * head, struct bytecode_list * call_node);
static void rename_constants(struct bytecode_list * to_inline, int n_constants_in_use_in_target);
static void copy_consants(struct function_object * target, struct function_object * to_inline);
static void remove_eof(struct bytecode_list * to_inline);
static void rename_monitors(struct function_object *, struct bytecode_list * to_inline);
static void resolve_pending_jumps(struct bytecode_list *to_inline_head, struct bytecode_list *target_first_instruction_after_inlined);
static void mark_jumps_as_pending_to_resolve(struct bytecode_list *, struct bytecode_list *);
static void move_jump_references_to_next_instruction(struct bytecode_list * head, struct bytecode_list * call_node);
static void add_packages_instructions(struct package * target_package, struct bytecode_list * chunk_to_inline, struct package * to_inline_package);

#define PENDING_TO_RESOLVE_RETURN ((void *) 0xffffffffffffffff)

//target <-- function_to_inline function_to_inline will get inlined in target
struct function_inline_result inline_function(
        struct function_object * target,
        int chunk_target_index,
        struct function_object * function_to_inline
) {
    struct bytecode_list * chunk_to_inline = create_bytecode_list(function_to_inline->chunk);
    struct bytecode_list * target_chunk = create_bytecode_list(target->chunk);
    struct bytecode_list * target_call = get_by_index_bytecode_list(target_chunk, chunk_target_index);
    struct bytecode_list * next_to_target_call = target_call->next;
    int n_arguments_to_inline = function_to_inline->n_arguments;
    int target_size_before_inlining = target->chunk->in_use;

    rename_constants(chunk_to_inline, target->chunk->constants.in_use);
    rename_local_variables(chunk_to_inline, target);
    remove_double_emtpy_return(chunk_to_inline);
    rename_return_statements(chunk_to_inline, target);
    remove_eof(chunk_to_inline);
    rename_monitors(function_to_inline, chunk_to_inline);

    rename_argument_passing(target, &target_chunk, chunk_target_index, n_arguments_to_inline);
    merge_to_inline_and_target(target_call, chunk_to_inline);
    resolve_pending_jumps(chunk_to_inline, next_to_target_call);
    copy_consants(target, function_to_inline);
    remove_op_call(target_chunk, target_call);

    target_chunk = get_first_bytecode_list(target_chunk);
    struct chunk * result_chunk = to_chunk_bytecode_list(target_chunk);
    result_chunk->constants = target->chunk->constants;
    result_chunk->lines = NULL;
    int target_size_after_inlining = target->chunk->in_use;

    free_bytecode_list(target_chunk);

    return (struct function_inline_result) {
        .total_size_added = target_size_after_inlining - target_size_before_inlining,
        .n_locals_added = function_to_inline->n_locals,
        .inlined_chunk = result_chunk,
    };
}

static void resolve_pending_jumps(
        struct bytecode_list * to_inline_head,
        struct bytecode_list * target_first_instruction_after_inlined
) {
    struct bytecode_list * current = to_inline_head;

    while (current != target_first_instruction_after_inlined) {
        if(is_jump_bytecode_instruction(current->bytecode) && current->as.jump == PENDING_TO_RESOLVE_RETURN){
            current->as.jump = target_first_instruction_after_inlined;
        }

        current = current->next;
    }
}

static void rename_monitors(struct function_object * function, struct bytecode_list * to_inline) {
    struct bytecode_list * current = to_inline;
    while(current != NULL){
        if (current->bytecode == OP_ENTER_MONITOR || current->bytecode == OP_EXIT_MONITOR) {
            struct monitor * monitor_to_enter = &function->monitors[current->as.u8];
            current->bytecode = current->bytecode == OP_ENTER_MONITOR ?
                    OP_ENTER_MONITOR_EXPLICIT : OP_EXIT_MONITOR_EXPLICIT;
            current->as.u64 = (uint64_t) monitor_to_enter;
        }

        current = current->next;
    }
}

static void remove_eof(struct bytecode_list * to_inline) {
    struct bytecode_list * current = to_inline;
    while(current != NULL) {
        struct bytecode_list * next_to_current = current->next;

        if(current->bytecode == OP_EOF){
            unlink_instruction_bytecode_list(current);
        }

        current = next_to_current;
    }
}

static void copy_consants(struct function_object * target, struct function_object * to_inline) {
    for(int i = 0; i < to_inline->chunk->constants.in_use; i++){
        lox_value_t current_constant = to_inline->chunk->constants.values[i];
        append_lox_arraylist(&target->chunk->constants, current_constant);
    }
}

static void rename_constants(struct bytecode_list * to_inline, int n_constants_in_use_in_target) {
    for(struct bytecode_list * current = to_inline; current != NULL; current = current->next){
        switch (current->bytecode) {
            case OP_CONSTANT:
            case OP_PACKAGE_CONST:
            case OP_DEFINE_GLOBAL:
            case OP_GET_GLOBAL:
            case OP_SET_GLOBAL:
            case OP_INITIALIZE_STRUCT:
            case OP_GET_STRUCT_FIELD:
            case OP_SET_STRUCT_FIELD:
                int current_constant_offset = current->as.u8;
                current->as.u8 = current_constant_offset + n_constants_in_use_in_target;
                break;
        }
    }
}

static void remove_op_call(struct bytecode_list * head, struct bytecode_list * call_node) {
    //Maybe some jump instructions point to OP_CALL. As OP_CALL will get removed we want it to point to the next instruction
    //which will be the first inlined code
    move_jump_references_to_next_instruction(head, call_node);

    unlink_instruction_bytecode_list(call_node);
}

static void move_jump_references_to_next_instruction(struct bytecode_list * head, struct bytecode_list * call_node) {
    struct bytecode_list * next_to_call_node = call_node->next;
    struct bytecode_list * current = head;

    while (current != call_node) {
        if(is_jump_bytecode_instruction(current->bytecode) && current->as.jump == call_node){
            current->as.jump = next_to_call_node;
        }

        current = current->next;
    }
}

static void merge_to_inline_and_target(struct bytecode_list * merge_node, struct bytecode_list * to_inline) {
    add_instructions_bytecode_list(merge_node, to_inline);
}

static void rename_argument_passing(
        struct function_object * target_function,
        struct bytecode_list ** target_chunk,
        int call_index,
        int n_arguments
) {
    if (n_arguments == 0) {
        return;
    }

    struct bytecode_list * call_node = get_by_index_bytecode_list(*target_chunk, call_index);

    struct stack_list stack;
    init_stack_list(&stack);
    get_call_args_in_stack(&stack, *target_chunk, target_function, call_index);

    for (int i = 0; i < n_arguments; i++) {
        struct bytecode_list * arg_node = pop_stack_list(&stack);
        struct bytecode_list * set_local = alloc_bytecode_list();
        set_local->bytecode = OP_SET_LOCAL;
        set_local->as.u8 = i + target_function->n_locals + 1; //All functions local variables starts with 1

        add_instruction_bytecode_list(arg_node, set_local);
        add_instruction_bytecode_list(set_local, create_instruction_bytecode_list(OP_POP));
    }

    //Remove OP_GET_GLOBAL <function name>
    struct bytecode_list * get_function_node = pop_stack_list(&stack);

    //This functinos belongs to a package, we also need to remove the enter/exit package instructions
    if (get_function_node->next->bytecode == OP_EXIT_PACKAGE) {
        if (*target_chunk == get_function_node ||
            *target_chunk == get_function_node->prev->prev ||
            *target_chunk == get_function_node->prev ||
            *target_chunk == get_function_node->next) {

            *target_chunk = get_function_node->next->next;
        }
        unlink_instruction_bytecode_list(get_function_node->prev->prev); //Unlink OP_PACKAGE_CONST
        unlink_instruction_bytecode_list(get_function_node->prev); //Unlink OP_ENTER_PACKAGE
        unlink_instruction_bytecode_list(get_function_node->next); //Unlink OP_EXIT_PACKAGE
    } else if (get_function_node == *target_chunk) {
        *target_chunk = get_function_node->next;
    }

    unlink_instruction_bytecode_list(get_function_node);

    free_stack_list(&stack);
}

static void get_call_args_in_stack(
        struct stack_list * stack,
        struct bytecode_list * target_chunk,
        struct function_object * target_function,
        int call_index
) {
    struct bytecode_list * call_node = get_by_index_bytecode_list(target_chunk, call_index);
    struct bytecode_list * current_node = target_chunk;

    while (current_node != call_node) {
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

        if(current_instruction == OP_NIL &&
           prev_instruction == OP_RETURN &&
           current_node->next->bytecode == OP_RETURN) {

            struct bytecode_list * op_return = current_node->next;
            struct bytecode_list * next_to_op_return = current_node->next->next;

            //Maybe some jumps point to OP_NIL, so we need to mark them as pending to resolve
            //So later, they will be resolved to point to the first instruction of target after the inlining instructions
            mark_jumps_as_pending_to_resolve(to_inline, current_node);

            unlink_instruction_bytecode_list(current_node); //OP_NIL
            unlink_instruction_bytecode_list(op_return); //OP_RETURN

            current_node = next_to_op_return;
        } else {
            prev_instruction = current_instruction;
            current_node = current_node->next;
        }
    }
}

//We mark jumps as unresolved if they refer to to_reference
static void mark_jumps_as_pending_to_resolve(struct bytecode_list * head, struct bytecode_list * to_reference) {
    struct bytecode_list * current = head;

    while(current != to_reference){
        if(is_jump_bytecode_instruction(current->bytecode) && current->as.jump == to_reference){
            current->as.jump = PENDING_TO_RESOLVE_RETURN;
        }

        current = current->next;
    }
}

static void rename_return_statements(struct bytecode_list * to_inline, struct function_object * target) {
    int return_local_variable_num = target->n_locals;

    struct bytecode_list * current_instruction = to_inline;

    while (current_instruction != NULL) {
        if (current_instruction->bytecode == OP_RETURN) {
            struct bytecode_list * next_to_current = current_instruction->next;

            if (next_to_current->bytecode == OP_EOF) {
                //The callee will always place the return value in the stack
                //The caller will always pop it from the stack or OP_POP instruction will be used
                //So we can discard return statements
                unlink_instruction_bytecode_list(current_instruction);
            } else {
                //We need to jump to the next of last instrution of to_inline after it has been inlined
                //But we don't know yet the offset, so we place 0Xffffffffffffffff to indice that we need to resolve it
                current_instruction->bytecode = OP_JUMP;
                current_instruction->as.jump = PENDING_TO_RESOLVE_RETURN;
            }

            current_instruction = next_to_current;
        } else {
            current_instruction = current_instruction->next;
        }
    }
}