#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/advanced/ssa_control_node.h"

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/types/package_object.h"
#include "shared/package.h"

#define READ_CONSTANT(function, bytecode) (function->chunk->constants.values[bytecode->as.u8])

static struct ssa_data_constant_node * create_constant_node(lox_value_t constant_value);

void create_ssa(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * function_bytecode
) {
    struct stack_list package_stack;
    struct stack_list data_nodes_stack;
    init_stack_list(&package_stack);
    init_stack_list(&data_nodes_stack);

    push_stack_list(&package_stack, package);

    struct ssa_control_start_node * start = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_START, struct ssa_control_start_node);
    struct ssa_control_node * last_control_node = (struct ssa_control_node *) start;

    for (struct bytecode_list * current = function_bytecode; function_bytecode != NULL; function_bytecode = function_bytecode->next) {
        switch (current->bytecode) {
            case OP_JUMP_IF_FALSE: break;
            case OP_JUMP: break;
            case OP_SET_LOCAL: break;
            case OP_LOOP: break;

            case OP_GET_LOCAL: {
                break;
            };

            case OP_SET_ARRAY_ELEMENT: {
                struct ssa_control_set_array_element_node * set_arrary_elemetn_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT, struct ssa_control_set_array_element_node
                );
                set_arrary_elemetn_node->index = current->as.u16;
                set_arrary_elemetn_node->array = pop_stack_list(&data_nodes_stack);
                set_arrary_elemetn_node->new_element = pop_stack_list(&data_nodes_stack);
                last_control_node->next.next = &set_arrary_elemetn_node->control;
                last_control_node = &set_arrary_elemetn_node->control;
                break;
            }
            case OP_SET_STRUCT_FIELD: {
                struct ssa_control_set_struct_field_node * set_struct_field = ALLOC_SSA_CONTROL_NODE(
                    SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD, struct ssa_control_set_struct_field_node
                );
                set_struct_field->field_name = AS_STRING_OBJECT(READ_CONSTANT(function, current));
                set_struct_field->field_value = pop_stack_list(&data_nodes_stack);
                set_struct_field->instance = pop_stack_list(&data_nodes_stack);
                last_control_node->next.next = &set_struct_field->control;
                last_control_node = &set_struct_field->control;
                break;
            }
            case OP_SET_GLOBAL: {
                struct ssa_control_set_global_node * set_global_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTORL_NODE_TYPE_SET_GLOBAL, struct ssa_control_set_global_node
                );

                set_global_node->name = AS_STRING_OBJECT(READ_CONSTANT(function, current));
                set_global_node->value_node = pop_stack_list(&data_nodes_stack);
                set_global_node->package = pop_stack_list(&package_stack);
                last_control_node->next.next = &set_global_node->control;
                last_control_node = &set_global_node->control;
                break;
            }

            case OP_EXIT_MONITOR_EXPLICIT: {
                struct monitor * monitor = (struct monitor *) current->as.u64;
                struct ssa_control_exit_monitor_node * exit_monitor_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_EXIT_MONITOR, struct ssa_control_exit_monitor_node
                );
                exit_monitor_node->monitor = monitor;
                last_control_node->next.next = &exit_monitor_node->control;
                last_control_node = &exit_monitor_node->control;
                break;
            }
            case OP_EXIT_MONITOR: {
                monitor_number_t monitor_number = current->as.u8;
                struct monitor * monitor = &function->monitors[monitor_number];
                struct ssa_control_exit_monitor_node * exit_monitor_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_EXIT_MONITOR, struct ssa_control_exit_monitor_node
                );
                exit_monitor_node->monitor = monitor;
                exit_monitor_node->monitor = monitor;
                last_control_node->next.next = &exit_monitor_node->control;
                last_control_node = &exit_monitor_node->control;
                break;
            }
            case OP_ENTER_MONITOR_EXPLICIT: {
                struct monitor * monitor = (struct monitor *) current->as.u64;
                struct ssa_control_enter_monitor_node * enter_monitor_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_ENTER_MONITOR, struct ssa_control_enter_monitor_node
                );
                enter_monitor_node->monitor = monitor;
                last_control_node->next.next = &enter_monitor_node->control;
                last_control_node = &enter_monitor_node->control;
                break;
            }
            case OP_ENTER_MONITOR: {
                monitor_number_t monitor_number = current->as.u8;
                struct monitor * monitor = &function->monitors[monitor_number];
                struct ssa_control_enter_monitor_node * enter_monitor_node = ALLOC_SSA_CONTROL_NODE(
                        SSA_CONTROL_NODE_TYPE_ENTER_MONITOR, struct ssa_control_enter_monitor_node
                );
                enter_monitor_node->monitor = monitor;
                last_control_node->next.next = &enter_monitor_node->control;
                last_control_node = &enter_monitor_node->control;
                break;
            }
            case OP_RETURN: {
                struct ssa_control_return_node * return_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_RETURN, struct ssa_control_return_node);
                return_node->data = pop_stack_list(&data_nodes_stack);
                last_control_node->next.next = &return_node->control;
                last_control_node = &return_node->control;
                break;
            }
            case OP_PRINT: {
                struct ssa_control_print_node * print_node = ALLOC_SSA_CONTROL_NODE(SSA_CONTROL_NODE_TYPE_PRINT, struct ssa_control_print_node);
                print_node->control.next.next = &print_node->control;
                print_node->data = pop_stack_list(&data_nodes_stack);
                last_control_node = &print_node->control;
                break;
            };

            //Expressions, data nodes
            case OP_CALL: {
                struct ssa_control_function_call_node * call_node = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_CALL, struct ssa_control_function_call_node);
                bool is_paralell = current->as.pair.u8_2;
                uint8_t n_args = current->as.pair.u8_1;
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

                break;
            }
            case OP_GET_GLOBAL: {
                struct ssa_data_get_global_node * get_global_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_GET_GLOBAL, struct ssa_data_get_global_node
                );
                get_global_node->package = peek_stack_list(&package_stack);
                get_global_node->name = AS_STRING_OBJECT(READ_CONSTANT(function, current));
                push_stack_list(&data_nodes_stack, get_global_node);
                break;
            }
            case OP_INITIALIZE_ARRAY: {
                struct ssa_data_initialize_array_node * initialize_array_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY, struct ssa_data_initialize_array_node
                );
                bool empty_initialization = current->as.initialize_array.is_emtpy_initializaion;
                uint16_t n_elements = current->as.initialize_array.n_elements;

                initialize_array_node->empty_initialization = empty_initialization;
                initialize_array_node->n_elements = n_elements;
                if(!empty_initialization){
                    initialize_array_node->elememnts_node = malloc(sizeof(struct ssa_data_node *) * n_elements);
                }
                for(int i = n_elements - 1; i >= 0 && !empty_initialization; i--){
                    initialize_array_node->elememnts_node[i] = pop_stack_list(&data_nodes_stack);
                }
                push_stack_list(&data_nodes_stack, initialize_array_node);
                break;
            }

            case OP_INITIALIZE_STRUCT: {
                struct ssa_data_initialize_struct_node * initialize_struct_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT, struct ssa_data_initialize_struct_node
                );
                struct struct_definition_object * definition = (struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(function, current));
                initialize_struct_node->fields_nodes = malloc(sizeof(struct struct_definition_object *) * definition->n_fields);
                initialize_struct_node->definition = definition;
                for (int i = definition->n_fields - 1; i >= 0; i--) {
                    initialize_struct_node->fields_nodes[i] = pop_stack_list(&data_nodes_stack);
                }
                push_stack_list(&data_nodes_stack, initialize_struct_node);
                break;
            };
            case OP_GET_STRUCT_FIELD: {
                struct ssa_data_get_struct_field_node * get_struct_field_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD, struct ssa_data_get_struct_field_node
                );
                get_struct_field_node->instance_node = pop_stack_list(&data_nodes_stack);
                get_struct_field_node->field_name = AS_STRING_OBJECT(READ_CONSTANT(function, current));
                push_stack_list(&data_nodes_stack, get_struct_field_node);
                break;
            }
            case OP_GET_ARRAY_ELEMENT: {
                struct ssa_data_get_array_element_node * get_array_element_node = ALLOC_SSA_DATA_NODE(
                        SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT, struct ssa_data_get_array_element_node
                );
                get_array_element_node->instance = pop_stack_list(&data_nodes_stack);
                get_array_element_node->index = current->as.u16;
                push_stack_list(&data_nodes_stack, get_array_element_node);
                break;
            }
            case OP_NEGATE:
            case OP_NOT: {
                struct ssa_data_unary_node * unary_node = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_UNARY, struct ssa_data_unary_node);
                unary_node->unary_operation_type = current->bytecode == OP_NEGATE ? UNARY_OPERATION_TYPE_NEGATION : UNARY_OPERATION_TYPE_NOT;
                unary_node->unary_value_node = pop_stack_list(&data_nodes_stack);
                push_stack_list(&data_nodes_stack, unary_node);
                break;
            }
            case OP_GREATER:
            case OP_LESS:
            case OP_EQUAL: {
                struct ssa_data_comparation_node * compartion_node = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_COMPARATION, struct ssa_data_comparation_node);
                struct ssa_data_node * right = pop_stack_list(&data_nodes_stack);
                struct ssa_data_node * left = pop_stack_list(&data_nodes_stack);
                compartion_node->operand = function_bytecode->bytecode;
                compartion_node->right_type = get_produced_type_ssa_data(right);
                compartion_node->left_type = get_produced_type_ssa_data(left);
                compartion_node->right = right;
                compartion_node->left = left;
                push_stack_list(&data_nodes_stack, compartion_node);
                break;
            };
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV: {
                struct ssa_data_arithmetic_node * arithmetic_node = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_ARITHMETIC, struct ssa_data_arithmetic_node);
                struct ssa_data_node * right = pop_stack_list(&data_nodes_stack);
                struct ssa_data_node * left = pop_stack_list(&data_nodes_stack);
                arithmetic_node->operand = function_bytecode->bytecode;
                arithmetic_node->right_type = get_produced_type_ssa_data(right);
                arithmetic_node->left_type = get_produced_type_ssa_data(left);
                arithmetic_node->left = left;
                arithmetic_node->right = right;
                push_stack_list(&data_nodes_stack, arithmetic_node);
                break;
            }
            case OP_CONSTANT: {
                push_stack_list(&data_nodes_stack, create_constant_node(READ_CONSTANT(function, current)));
                break;
            }
            case OP_FALSE: {
                push_stack_list(&data_nodes_stack, create_constant_node(TAG_FALSE));
                break;
            }
            case OP_TRUE: {
                push_stack_list(&data_nodes_stack, create_constant_node(TAG_TRUE));
                break;
            }
            case OP_NIL: {
                push_stack_list(&data_nodes_stack, create_constant_node(TAG_NIL));
                break;
            }
            case OP_FAST_CONST_8: {
                push_stack_list(&data_nodes_stack, create_constant_node(AS_NUMBER(current->as.u8)));
                break;
            }
            case OP_FAST_CONST_16: {
                push_stack_list(&data_nodes_stack, create_constant_node(AS_NUMBER(current->as.u16)));
                break;
            }
            case OP_CONST_1: {
                push_stack_list(&data_nodes_stack, create_constant_node(AS_NUMBER(1)));
                break;
            }
            case OP_CONST_2: {
                push_stack_list(&data_nodes_stack, create_constant_node(AS_NUMBER(2)));
                break;
            }
            case OP_PACKAGE_CONST: {
                lox_value_t package_lox_value = READ_CONSTANT(function, current);
                struct package_object * package_const = (struct package_object *) AS_OBJECT(package_lox_value);
                push_stack_list(&package_stack, package_const->package);
                break;
            }
            case OP_ENTER_PACKAGE: break;
            case OP_EXIT_PACKAGE: {
                pop_stack_list(&package_stack);
                break;
            }
            case OP_DEFINE_GLOBAL:
            case OP_NO_OP:
            case OP_POP:
            case OP_EOF:
                break;
        }
    }
}

static struct ssa_data_constant_node * create_constant_node(lox_value_t constant_value) {
    struct ssa_data_constant_node * constant_node = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_CONSTANT, struct ssa_data_constant_node);

    switch (get_lox_type(constant_value)) {
        case VAL_BOOL:
            constant_node->as.boolean = AS_BOOL(constant_value);
            constant_node->type = SSA_DATA_TYPE_BOOLEAN;
            break;

        case VAL_NIL: {
            constant_node->as.nil = NULL;
            constant_node->type = SSA_DATA_TYPE_NIL;
            break;
        }

        case VAL_NUMBER: {
            if(has_decimals(AS_NUMBER(constant_value))){
                constant_node->as.f64 = AS_NUMBER(constant_value);
                constant_node->type = SSA_DATA_TYPE_F64;
            } else {
                constant_node->as.i64 = (int64_t) constant_value;
                constant_node->type = SSA_DATA_TYPE_I64;
            }
            break;
        }

        case VAL_OBJ: {
            if(IS_STRING(constant_value)){
                constant_node->as.string = AS_STRING_OBJECT(constant_value);
                constant_node->type = SSA_DATA_TYPE_STRING;
            } else {
                constant_node->as.object = AS_OBJECT(constant_value);
                constant_node->type = SSA_DATA_TYPE_OBJECT;
            }
            break;
        }
    }

    return constant_node;
}