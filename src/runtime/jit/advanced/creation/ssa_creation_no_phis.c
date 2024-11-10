#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/ssa_control_node.h"

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/package.h"

//This file will expoase the function "create_ssa_ir_no_phis", given a bytecodelist, it will output the ssa graph ir
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

static struct ssa_data_constant_node * create_constant_node(lox_value_t constant_value, struct bytecode_list *);
static void push_pending_evaluate(struct stack_list *, pending_evaluation_type_t, struct bytecode_list *, struct ssa_control_node *);
static void attatch_ssa_node_to_parent(pending_evaluation_type_t type, struct ssa_control_node * parent, struct ssa_control_node * child);
static struct bytecode_list * simplify_redundant_unconditional_jump_bytecodes(struct bytecode_list *bytecode_list);
static void map_data_nodes_bytecodes_to_control(
        struct u64_hash_table * control_ssa_nodes_by_bytecode,
        struct ssa_data_node * data_node,
        struct ssa_control_node * to_map_control
);

struct ssa_control_node * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
) {
    struct function_profile_data * function_profile = &function->state_as.profiling.profile_data;
    struct u64_hash_table control_nodes_by_bytecode;
    struct stack_list pending_evaluation;
    struct stack_list data_nodes_stack;
    struct stack_list package_stack;
    init_u64_hash_table(&control_nodes_by_bytecode);
    init_stack_list(&pending_evaluation);
    init_stack_list(&data_nodes_stack);
    init_stack_list(&package_stack);

    push_stack_list(&package_stack, package);

    struct ssa_control_start_node * start_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_START, struct ssa_control_start_node);
    push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, start_function_bytecode, &start_node->control);

    while (!is_empty_stack_list(&pending_evaluation)) {
        struct pending_evaluate * pending_evaluate = pop_stack_list(&pending_evaluation);
        struct bytecode_list * current_bytecode_to_evaluate = pending_evaluate->pending_bytecode;
        struct ssa_control_node * parent_ssa_control_node = pending_evaluate->parent_ssa_node;
        pending_evaluation_type_t evaluation_type = pending_evaluate->type;
        free(pending_evaluate);

        if (contains_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate)) {
            struct ssa_control_node * already_evaluted_node = get_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate);
            bool is_from_jump = evaluation_type == EVAL_TYPE_JUMP_SEQUENTIAL_CONTROL;
            pending_evaluation_type_t next_parent_type = is_from_jump ? EVAL_TYPE_JUMP_SEQUENTIAL_CONTROL: EVAL_TYPE_SEQUENTIAL_CONTROL;
            attatch_ssa_node_to_parent(next_parent_type, parent_ssa_control_node, already_evaluted_node);
            continue;
        }

        switch (current_bytecode_to_evaluate->bytecode) {
            case OP_JUMP_IF_FALSE: {
                struct ssa_control_conditional_jump_node * cond_jump_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP, struct ssa_control_conditional_jump_node
                );
                struct ssa_data_node * condition = pop_stack_list(&data_nodes_stack);

                cond_jump_node->condition = condition;
                struct bytecode_list * false_branch_bytecode = current_bytecode_to_evaluate->as.jump;
                struct bytecode_list * true_branch_bytecode = current_bytecode_to_evaluate->next;

                map_data_nodes_bytecodes_to_control(&control_nodes_by_bytecode, condition, &cond_jump_node->control);
                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &cond_jump_node->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_CONDITIONAL_JUMP_FALSE_BRANCH_CONTROL, false_branch_bytecode, &cond_jump_node->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_CONDITIONAL_JUMP_TRUE_BRANCH_CONTROL, true_branch_bytecode, &cond_jump_node->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, cond_jump_node);
                break;
            }
            case OP_JUMP: {
                //OP_JUMP are translated to edges in the ssa ir graph
                struct bytecode_list * to_jump_bytecode = simplify_redundant_unconditional_jump_bytecodes(current_bytecode_to_evaluate->as.jump);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_JUMP_SEQUENTIAL_CONTROL, to_jump_bytecode, parent_ssa_control_node);
                break;
            }
            case OP_LOOP: {
                struct ssa_control_loop_jump_node * loop_jump_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_LOOP_JUMP, struct ssa_control_loop_jump_node);
                struct bytecode_list * to_jump_bytecode = current_bytecode_to_evaluate->as.jump;
                struct ssa_control_node * to_jump_ssa_node = get_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) to_jump_bytecode);

                if(to_jump_ssa_node->type == SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP){
                    struct ssa_control_conditional_jump_node * loop_condition_node = (struct ssa_control_conditional_jump_node *) to_jump_ssa_node;
                    loop_condition_node->loop_condition = true;
                }

                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &loop_jump_node->control);
                loop_jump_node->control.next.next = to_jump_ssa_node;
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, loop_jump_node);
                break;
            }
            case OP_POP: {
                //Expression statements
                if(!is_empty_stack_list(&data_nodes_stack)){
                    struct ssa_data_node * data_node = pop_stack_list(&data_nodes_stack);
                    struct ssa_control_data_node * control_data_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_DATA, struct ssa_control_data_node);

                    control_data_node->data = data_node;

                    map_data_nodes_bytecodes_to_control(&control_nodes_by_bytecode, data_node, &control_data_node->control);
                    attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &control_data_node->control);
                    push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, current_bytecode_to_evaluate->next, &control_data_node->control);
                    put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, control_data_node);
                }
                break;
            }
            case OP_SET_ARRAY_ELEMENT: {
                struct ssa_control_set_array_element_node * set_arrary_element_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT, struct ssa_control_set_array_element_node
                );
                struct ssa_data_node * instance = pop_stack_list(&data_nodes_stack);
                struct ssa_data_node * new_element = pop_stack_list(&data_nodes_stack);

                set_arrary_element_node->index = current_bytecode_to_evaluate->as.u16;
                set_arrary_element_node->new_element = new_element;
                set_arrary_element_node->array = instance;

                map_data_nodes_bytecodes_to_control(&control_nodes_by_bytecode, instance, &set_arrary_element_node->control);
                map_data_nodes_bytecodes_to_control(&control_nodes_by_bytecode, new_element, &set_arrary_element_node->control);
                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &set_arrary_element_node->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, current_bytecode_to_evaluate->next, &set_arrary_element_node->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, set_arrary_element_node);
                break;
            }
            case OP_SET_STRUCT_FIELD: {
                struct ssa_control_set_struct_field_node * set_struct_field = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD, struct ssa_control_set_struct_field_node
                );
                struct ssa_data_node * field_value = pop_stack_list(&data_nodes_stack);
                struct ssa_data_node * instance = pop_stack_list(&data_nodes_stack);

                set_struct_field->field_name = AS_STRING_OBJECT(READ_CONSTANT(function, current_bytecode_to_evaluate));
                set_struct_field->field_value = field_value;
                set_struct_field->instance = instance;

                map_data_nodes_bytecodes_to_control(&control_nodes_by_bytecode, field_value, &set_struct_field->control);
                map_data_nodes_bytecodes_to_control(&control_nodes_by_bytecode, instance, &set_struct_field->control);
                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &set_struct_field->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, current_bytecode_to_evaluate->next, &set_struct_field->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, set_struct_field);
                break;
            }
            case OP_SET_GLOBAL: {
                struct ssa_control_set_global_node * set_global_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTORL_NODE_TYPE_SET_GLOBAL, struct ssa_control_set_global_node
                );

                set_global_node->name = AS_STRING_OBJECT(READ_CONSTANT(function, current_bytecode_to_evaluate));
                set_global_node->value_node = pop_stack_list(&data_nodes_stack);
                set_global_node->package = peek_stack_list(&package_stack);

                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &set_global_node->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, current_bytecode_to_evaluate->next, &set_global_node->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, set_global_node);
                break;
            }
            case OP_EXIT_MONITOR_EXPLICIT: {
                struct monitor * monitor = (struct monitor *) current_bytecode_to_evaluate->as.u64;
                struct ssa_control_exit_monitor_node * exit_monitor_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_EXIT_MONITOR, struct ssa_control_exit_monitor_node
                );

                exit_monitor_node->monitor = monitor;

                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &exit_monitor_node->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, current_bytecode_to_evaluate->next, &exit_monitor_node->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, exit_monitor_node);
                break;
            }
            case OP_EXIT_MONITOR: {
                monitor_number_t monitor_number = current_bytecode_to_evaluate->as.u8;
                struct monitor * monitor = &function->monitors[monitor_number];
                struct ssa_control_exit_monitor_node * exit_monitor_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_EXIT_MONITOR, struct ssa_control_exit_monitor_node
                );

                exit_monitor_node->monitor = monitor;

                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &exit_monitor_node->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, current_bytecode_to_evaluate->next, &exit_monitor_node->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, exit_monitor_node);
                break;
            }
            case OP_ENTER_MONITOR_EXPLICIT: {
                struct monitor * monitor = (struct monitor *) current_bytecode_to_evaluate->as.u64;
                struct ssa_control_enter_monitor_node * enter_monitor_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_ENTER_MONITOR, struct ssa_control_enter_monitor_node
                );

                enter_monitor_node->monitor = monitor;

                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &enter_monitor_node->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, current_bytecode_to_evaluate->next, &enter_monitor_node->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, enter_monitor_node);
                break;
            }
            case OP_ENTER_MONITOR: {
                monitor_number_t monitor_number = current_bytecode_to_evaluate->as.u8;
                struct monitor * monitor = &function->monitors[monitor_number];
                struct ssa_control_enter_monitor_node * enter_monitor_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_ENTER_MONITOR, struct ssa_control_enter_monitor_node
                );

                enter_monitor_node->monitor = monitor;

                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &enter_monitor_node->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, current_bytecode_to_evaluate->next, &enter_monitor_node->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, enter_monitor_node);
                break;
            }
            case OP_RETURN: {
                struct ssa_control_return_node * return_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_RETURN, struct ssa_control_return_node);
                struct ssa_data_node * return_value = pop_stack_list(&data_nodes_stack);

                return_node->data = return_value;

                map_data_nodes_bytecodes_to_control(&control_nodes_by_bytecode, return_value, &return_node->control);
                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &return_node->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, return_node);
                break;
            }
            case OP_PRINT: {
                struct ssa_control_print_node * print_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_PRINT, struct ssa_control_print_node);
                struct ssa_data_node * print_value = pop_stack_list(&data_nodes_stack);

                print_node->data = print_value;

                map_data_nodes_bytecodes_to_control(&control_nodes_by_bytecode, print_value, &print_node->control);
                attatch_ssa_node_to_parent(evaluation_type, parent_ssa_control_node, &print_node->control);
                push_pending_evaluate(&pending_evaluation, EVAL_TYPE_SEQUENTIAL_CONTROL, current_bytecode_to_evaluate->next, &print_node->control);
                put_u64_hash_table(&control_nodes_by_bytecode, (uint64_t) current_bytecode_to_evaluate, print_node);
                break;
            }
            //Expressions, data nodes
            case OP_SET_LOCAL: {
                struct ssa_data_set_local_node * set_local_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_SET_LOCAL, struct ssa_data_set_local_node, current_bytecode_to_evaluate
                );
                set_local_node->new_local_value = pop_stack_list(&data_nodes_stack);
                set_local_node->local_number = current_bytecode_to_evaluate->as.u8;

                push_stack_list(&data_nodes_stack, set_local_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_GET_LOCAL: {
                struct ssa_data_get_local_node * get_local_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_GET_LOCAL, struct ssa_data_get_local_node, current_bytecode_to_evaluate
                );

                //Pending initialize
                int local_number = current_bytecode_to_evaluate->as.u8;
                get_local_node->local_number = local_number;
                get_local_node->type = get_type_by_local_function_profile_data(&function->state_as.profiling.profile_data, local_number);
                get_local_node->n_phi_numbers_elements = 0;
                get_local_node->phi_numbers = NULL;

                push_stack_list(&data_nodes_stack, get_local_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_CALL: {
                struct ssa_control_function_call_node * call_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_CALL, struct ssa_control_function_call_node, current_bytecode_to_evaluate
                );
                bool is_paralell = current_bytecode_to_evaluate->as.pair.u8_2;
                uint8_t n_args = current_bytecode_to_evaluate->as.pair.u8_1;
                struct ssa_data_node * function_to_call = peek_n_stack_list(&data_nodes_stack, n_args);

                call_node->function = function_to_call;
                call_node->is_parallel = is_paralell;
                call_node->n_arguments = n_args;
                call_node->arguments = malloc(sizeof(struct ssa_data_node *) * n_args);
                for(int i = n_args; i > 0; i--){
                    call_node->arguments[i - 1] = pop_stack_list(&data_nodes_stack);
                }
                //Remove function object in the stack
                pop_stack_list(&data_nodes_stack);

                push_stack_list(&data_nodes_stack, call_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_GET_GLOBAL: {
                struct ssa_data_get_global_node * get_global_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_GET_GLOBAL, struct ssa_data_get_global_node, current_bytecode_to_evaluate
                );
                get_global_node->package = peek_stack_list(&package_stack);
                get_global_node->name = AS_STRING_OBJECT(READ_CONSTANT(function, current_bytecode_to_evaluate));

                push_stack_list(&data_nodes_stack, get_global_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_INITIALIZE_ARRAY: {
                struct ssa_data_initialize_array_node * initialize_array_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY, struct ssa_data_initialize_array_node, current_bytecode_to_evaluate
                );
                bool empty_initialization = current_bytecode_to_evaluate->as.initialize_array.is_emtpy_initializaion;
                uint16_t n_elements = current_bytecode_to_evaluate->as.initialize_array.n_elements;

                initialize_array_node->empty_initialization = empty_initialization;
                initialize_array_node->n_elements = n_elements;
                if(!empty_initialization){
                    initialize_array_node->elememnts_node = malloc(sizeof(struct ssa_data_node *) * n_elements);
                }
                for(int i = n_elements - 1; i >= 0 && !empty_initialization; i--){
                    initialize_array_node->elememnts_node[i] = pop_stack_list(&data_nodes_stack);
                }

                push_stack_list(&data_nodes_stack, initialize_array_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_INITIALIZE_STRUCT: {
                struct ssa_data_initialize_struct_node * initialize_struct_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT, struct ssa_data_initialize_struct_node, current_bytecode_to_evaluate
                );
                struct struct_definition_object * definition = (struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(function, current_bytecode_to_evaluate));
                initialize_struct_node->fields_nodes = malloc(sizeof(struct struct_definition_object *) * definition->n_fields);
                initialize_struct_node->definition = definition;
                for (int i = definition->n_fields - 1; i >= 0; i--) {
                    initialize_struct_node->fields_nodes[i] = pop_stack_list(&data_nodes_stack);
                }

                push_stack_list(&data_nodes_stack, initialize_struct_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_GET_STRUCT_FIELD: {
                struct ssa_data_get_struct_field_node * get_struct_field_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD, struct ssa_data_get_struct_field_node, current_bytecode_to_evaluate
                );
                get_struct_field_node->instance_node = pop_stack_list(&data_nodes_stack);
                get_struct_field_node->field_name = AS_STRING_OBJECT(READ_CONSTANT(function, current_bytecode_to_evaluate));

                push_stack_list(&data_nodes_stack, get_struct_field_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_GET_ARRAY_ELEMENT: {
                struct ssa_data_get_array_element_node * get_array_element_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT, struct ssa_data_get_array_element_node, current_bytecode_to_evaluate
                );
                get_array_element_node->instance = pop_stack_list(&data_nodes_stack);
                get_array_element_node->index = current_bytecode_to_evaluate->as.u16;

                push_stack_list(&data_nodes_stack, get_array_element_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_NEGATE:
            case OP_NOT: {
                struct ssa_data_unary_node * unary_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_UNARY, struct ssa_data_unary_node, current_bytecode_to_evaluate
                );
                unary_node->unary_operation_type = current_bytecode_to_evaluate->bytecode == OP_NEGATE ? UNARY_OPERATION_TYPE_NEGATION : UNARY_OPERATION_TYPE_NOT;
                unary_node->unary_value_node = pop_stack_list(&data_nodes_stack);

                push_stack_list(&data_nodes_stack, unary_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_GREATER:
            case OP_LESS:
            case OP_EQUAL: {
                struct ssa_data_comparation_node * compartion_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_COMPARATION, struct ssa_data_comparation_node, current_bytecode_to_evaluate
                );
                struct ssa_data_node * right = pop_stack_list(&data_nodes_stack);
                struct ssa_data_node * left = pop_stack_list(&data_nodes_stack);
                compartion_node->operand = start_function_bytecode->bytecode;
                compartion_node->right_type = get_produced_type_ssa_data(function_profile, right);
                compartion_node->left_type = get_produced_type_ssa_data(function_profile, left);
                compartion_node->right = right;
                compartion_node->left = left;

                push_stack_list(&data_nodes_stack, compartion_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV: {
                struct ssa_data_arithmetic_node * arithmetic_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_ARITHMETIC, struct ssa_data_arithmetic_node, current_bytecode_to_evaluate
                );
                struct ssa_data_node * right = pop_stack_list(&data_nodes_stack);
                struct ssa_data_node * left = pop_stack_list(&data_nodes_stack);
                arithmetic_node->operand = start_function_bytecode->bytecode;
                arithmetic_node->right_type = get_produced_type_ssa_data(function_profile, right);
                arithmetic_node->left_type = get_produced_type_ssa_data(function_profile, left);
                arithmetic_node->left = left;
                arithmetic_node->right = right;

                push_stack_list(&data_nodes_stack, arithmetic_node);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);

                break;
            }
            case OP_CONSTANT: {
                push_stack_list(&data_nodes_stack, create_constant_node(READ_CONSTANT(function, current_bytecode_to_evaluate), current_bytecode_to_evaluate));
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_FALSE: {
                push_stack_list(&data_nodes_stack, create_constant_node(TAG_FALSE, current_bytecode_to_evaluate));
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_TRUE: {
                push_stack_list(&data_nodes_stack, create_constant_node(TAG_TRUE, current_bytecode_to_evaluate));
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_NIL: {
                push_stack_list(&data_nodes_stack, create_constant_node(TAG_NIL, current_bytecode_to_evaluate));
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_FAST_CONST_8: {
                push_stack_list(&data_nodes_stack, create_constant_node(AS_NUMBER(current_bytecode_to_evaluate->as.u8), current_bytecode_to_evaluate));
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_FAST_CONST_16: {
                push_stack_list(&data_nodes_stack, create_constant_node(AS_NUMBER(current_bytecode_to_evaluate->as.u16), current_bytecode_to_evaluate));
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_CONST_1: {
                push_stack_list(&data_nodes_stack, create_constant_node(AS_NUMBER(1), current_bytecode_to_evaluate));
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_CONST_2: {
                push_stack_list(&data_nodes_stack, create_constant_node(AS_NUMBER(2), current_bytecode_to_evaluate));
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_PACKAGE_CONST: {
                lox_value_t package_lox_value = READ_CONSTANT(function, current_bytecode_to_evaluate);
                struct package_object * package_const = (struct package_object *) AS_OBJECT(package_lox_value);

                push_stack_list(&package_stack, package_const->package);
                push_pending_evaluate(&pending_evaluation, evaluation_type, current_bytecode_to_evaluate->next, parent_ssa_control_node);
                break;
            }
            case OP_ENTER_PACKAGE: break;
            case OP_EXIT_PACKAGE: {
                pop_stack_list(&package_stack);
                break;
            }
            case OP_DEFINE_GLOBAL:
            case OP_NO_OP:
            case OP_EOF:
                break;
        }
    }

    free_u64_hash_table(&control_nodes_by_bytecode);
    free_stack_list(&pending_evaluation);
    free_stack_list(&data_nodes_stack);
    free_stack_list(&package_stack);

    return &start_node->control;
}

static struct ssa_data_constant_node * create_constant_node(lox_value_t constant_value, struct bytecode_list * bytecode) {
    struct ssa_data_constant_node * constant_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_CONSTANT, struct ssa_data_constant_node, bytecode
    );

    switch (get_lox_type(constant_value)) {
        case VAL_BOOL:
            constant_node->as.boolean = AS_BOOL(constant_value);
            constant_node->type = PROFILE_DATA_TYPE_BOOLEAN;
            break;

        case VAL_NIL: {
            constant_node->as.nil = NULL;
            constant_node->type = PROFILE_DATA_TYPE_NIL;
            break;
        }

        case VAL_NUMBER: {
            if(has_decimals(AS_NUMBER(constant_value))){
                constant_node->as.f64 = AS_NUMBER(constant_value);
                constant_node->type = PROFILE_DATA_TYPE_F64;
            } else {
                constant_node->as.i64 = (int64_t) constant_value;
                constant_node->type = PROFILE_DATA_TYPE_I64;
            }
            break;
        }

        case VAL_OBJ: {
            if(IS_STRING(constant_value)){
                constant_node->as.string = AS_STRING_OBJECT(constant_value);
                constant_node->type = PROFILE_DATA_TYPE_STRING;
            } else {
                constant_node->as.object = AS_OBJECT(constant_value);
                constant_node->type = PROFILE_DATA_TYPE_OBJECT;
            }
            break;
        }
    }

    return constant_node;
}

static void push_pending_evaluate(
        struct stack_list * pending_evaluation_stack,
        pending_evaluation_type_t type,
        struct bytecode_list * pending_bytecode,
        struct ssa_control_node * parent_ssa_node
) {
    if (pending_bytecode != NULL) {
        struct pending_evaluate * pending_evalutaion = malloc(sizeof(struct pending_evaluate));
        pending_evalutaion->pending_bytecode = pending_bytecode;
        pending_evalutaion->parent_ssa_node = parent_ssa_node;
        pending_evalutaion->type = type;
        push_stack_list(pending_evaluation_stack, pending_evalutaion);
    }
}

static void attatch_ssa_node_to_parent(
        pending_evaluation_type_t type,
        struct ssa_control_node * parent,
        struct ssa_control_node * child
) {
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

//This function will map the data node bytecode to the to_map_control control node
//Example: Given OP_CONST_1, OP_CONST_2, OP_ADD, OP_PRINT
//The first 3 bytecodes will point to OP_PRINT
static void map_data_nodes_bytecodes_to_control(
        struct u64_hash_table * control_ssa_nodes_by_bytecode,
        struct ssa_data_node * data_node,
        struct ssa_control_node * to_map_control
) {
    //Add data_node
    put_u64_hash_table(control_ssa_nodes_by_bytecode, (uint64_t) data_node->original_bytecode, to_map_control);

    switch(data_node->type) {
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_control_function_call_node * call_node = (struct ssa_control_function_call_node *) data_node;

            for(int i = 0; i < call_node->n_arguments; i++) {
                map_data_nodes_bytecodes_to_control(
                        control_ssa_nodes_by_bytecode,
                        call_node->arguments[i],
                        to_map_control
                );
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_COMPARATION: {
            struct ssa_data_comparation_node * comparation_node = (struct ssa_data_comparation_node *) data_node;
            map_data_nodes_bytecodes_to_control(control_ssa_nodes_by_bytecode, comparation_node->right, to_map_control);
            map_data_nodes_bytecodes_to_control(control_ssa_nodes_by_bytecode, comparation_node->left, to_map_control);
            break;
        }
        case SSA_DATA_NODE_TYPE_ARITHMETIC: {
            struct ssa_data_arithmetic_node * arithmetic_node = (struct ssa_data_arithmetic_node *) data_node;
            map_data_nodes_bytecodes_to_control(control_ssa_nodes_by_bytecode, arithmetic_node->right, to_map_control);
            map_data_nodes_bytecodes_to_control(control_ssa_nodes_by_bytecode, arithmetic_node->left, to_map_control);
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary_node = (struct ssa_data_unary_node *) data_node;
            map_data_nodes_bytecodes_to_control(control_ssa_nodes_by_bytecode, unary_node->unary_value_node, to_map_control);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) data_node;
            map_data_nodes_bytecodes_to_control(control_ssa_nodes_by_bytecode, get_struct_field->instance_node, to_map_control);
            break;
        }
        case SSA_DATA_NODE_TYPE_SET_LOCAL: {
            struct ssa_data_set_local_node * set_local_node = (struct ssa_data_set_local_node *) data_node;
            map_data_nodes_bytecodes_to_control(control_ssa_nodes_by_bytecode, set_local_node->new_local_value, to_map_control);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * initilize_struct_node = (struct ssa_data_initialize_struct_node *) data_node;
            for(int i = 0; i < initilize_struct_node->definition->n_fields; i++){
                map_data_nodes_bytecodes_to_control(
                        control_ssa_nodes_by_bytecode,
                        initilize_struct_node->fields_nodes[i],
                        to_map_control
                );
            }

            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) data_node;
            map_data_nodes_bytecodes_to_control(control_ssa_nodes_by_bytecode, get_array_element->instance, to_map_control);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * initialize_array_node = (struct ssa_data_initialize_array_node *) data_node;
            for(int i = 0; i < initialize_array_node->n_elements && !initialize_array_node->empty_initialization; i++){
                map_data_nodes_bytecodes_to_control(
                        control_ssa_nodes_by_bytecode,
                        initialize_array_node->elememnts_node[i],
                        to_map_control
                );
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_CONSTANT:
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            break;
        }
    }
}