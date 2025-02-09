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
        int n_args,
        ... //Arguments
);

void emit_function_call_with_return_value_ll_lox_ir(
        struct lllil_control *lllil,
        void * function_address,
        struct v_register, //Return value register
        int n_args,
        ... //Arguments
);

void emit_function_call_ll_lox_ir(
        struct lllil_control *lllil,
        void * function_address,
        int n_args,
        ... //Arguments
);

void emit_unary_ll_lox_ir(
        struct lllil_control * lllil,
        struct lox_ir_ll_operand a,
        unary_operator_type_ll_lox_ir binary_operator
);

void emit_comparation_ll_lox_ir(
        struct lllil_control * lllil,
        comparation_operator_type_ll_lox_ir comparation_type,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
);

void emit_binary_ll_lox_ir(
        struct lllil_control * lllil,
        binary_operator_type_ll_lox_ir binary_operator,
        struct lox_ir_ll_operand result,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
);

void emit_range_check_ll_lox_ir(
        struct lllil_control *,
        struct lox_ir_ll_operand instance,
        struct lox_ir_ll_operand index
);

//result = a + b
void emit_string_concat_ll_lox_ir(
        struct lllil_control *,
        struct lox_ir_ll_operand result,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
);
