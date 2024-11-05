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

    for (struct bytecode_list * current = function_bytecode; function_bytecode != NULL; function_bytecode = function_bytecode->next) {
        switch (current->bytecode) {
            case OP_RETURN: break;
            case OP_PRINT: break;
            case OP_GET_GLOBAL: break;
            case OP_SET_GLOBAL: break;
            case OP_GET_LOCAL: break;
            case OP_JUMP_IF_FALSE: break;
            case OP_JUMP: break;
            case OP_SET_LOCAL: break;
            case OP_LOOP: break;
            case OP_CALL: break;
            case OP_INITIALIZE_STRUCT: break;
            case OP_GET_STRUCT_FIELD: break;
            case OP_SET_STRUCT_FIELD: break;
            case OP_ENTER_MONITOR: break;
            case OP_ENTER_MONITOR_EXPLICIT: break;
            case OP_EXIT_MONITOR_EXPLICIT: break;
            case OP_EXIT_MONITOR: break;
            case OP_INITIALIZE_ARRAY: break;
            case OP_GET_ARRAY_ELEMENT: break;
            case OP_SET_ARRAY_ELEMENT: break;

            case OP_NEGATE:
            case OP_NOT: {
                struct ssa_data_unary_node * unary_node = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYP<E_UNARY, struct ssa_data_unary_node);
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