#pragma once

#include "runtime/jit/advanced/ir_lowerer/lllil.h"
#include "runtime/jit/advanced/lox_ir_control_node.h"

#include "shared/types/struct_instance_object.h"
#include "shared.h"

int get_offset_field_struct_definition_ll_lox_ir(struct struct_definition_object *definition, char *field_name);

//move a, b a will be moved to b
void emit_move_ll_lox_ir(
        struct lllil_control *,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
);

//move base + offset, value
void emit_store_at_offset_ll_lox_ir(
        struct lllil_control *,
        struct lox_ir_ll_operand base,
        int offset,
        struct lox_ir_ll_operand value
);

//move value, base + offset
void emit_load_at_offset_ll_lox_ir(
        struct lllil_control *,
        struct lox_ir_ll_operand value,
        struct lox_ir_ll_operand base,
        int offset
);

struct lox_ir_ll_operand emit_lox_object_ptr_to_native_ll_lox_ir(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand input //Expect v register
);

struct lox_ir_ll_operand emit_guard_ll_lox_ir(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand guard_input,
        struct lox_ir_guard guard
);

void emit_conditional_function_call_ll_lox_ir(
        struct lllil_control *lllil,
        comparation_operator_type_ll_lox_ir condition,
        void * function_address,
        char * function_name,
        int n_args,
        ... //Arguments
);

void emit_function_call_with_return_value_ll_lox_ir(
        struct lllil_control *lllil,
        void * function_address,
        char * function_name,
        struct v_register, //Return value register
        int n_args,
        ... //Arguments
);

void emit_function_call_ll_lox_ir(
        struct lllil_control *lllil,
        void * function_address,
        char * function_name,
        int n_args,
        ... //Arguments
);

void emit_unary_ll_lox_ir(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand operand,
        unary_operator_type_ll_lox_ir operator
);

void emit_comparation_ll_lox_ir(
        struct lllil_control * lllil,
        comparation_operator_type_ll_lox_ir comparation_operator,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right
);

void emit_binary_ll_lox_ir(
        struct lllil_control * lllil,
        binary_operator_type_ll_lox_ir binary_operator,
        struct lox_ir_ll_operand result,
        struct lox_ir_ll_operand left,
        struct lox_ir_ll_operand right
);

struct lox_ir_ll_operand emit_get_array_length_ll_lox_ir(
        struct lllil_control *,
        struct lox_ir_ll_operand instance, //Expect type NATIVE_ARRAY_INSTANCE
        bool escapes,
        struct v_register result
);

void emit_range_check_ll_lox_ir(
        struct lllil_control *,
        struct lox_ir_ll_operand instance, //Expect type NATIVE_ARRAY_INSTANCE
        struct lox_ir_ll_operand index_to_access,
        bool escapes
);
