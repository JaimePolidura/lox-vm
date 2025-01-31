#pragma once

#include "runtime/jit/advanced/ir_lowerer/lllil.h"
#include "runtime/jit/advanced/lox_ir_control_node.h"
#include "shared.h"

int get_offset_field_struct_definition_ll_lox_ir(struct struct_definition_object *definition, char *field_name);

void emit_move_ll_lox_ir(
        struct lllil *,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
);

void emit_isub_ll_lox_ir(
        struct lllil *,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
);

//move base + offset, value
void emit_store_at_offset_ll_lox_ir(
        struct lllil *,
        struct lox_ir_control_node *,
        struct lox_ir_ll_operand base,
        int offset,
        struct lox_ir_ll_operand value
);

//move lox_ir_ll_operand, base + offset
struct lox_ir_ll_operand emit_load_at_offset_ll_lox_ir(
        struct lllil *,
        struct lox_ir_control_node *,
        struct lox_ir_ll_operand base,
        int offset
);

void emit_function_call_ll_lox_ir(
        struct lllil *lllil,
        struct lox_ir_control_node *control_node,
        void * function_address,
        int n_args,
        ... //Arguments
);

//integer multiplication, a = a * b
void emit_imul_ll_lox_ir(
        struct lllil *lllil,
        struct lox_ir_control_node *control_node,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
);

//integer addition, a = a * b
void emit_iadd_ll_lox_ir(
        struct lllil *lllil,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
);

void emit_guard_ll_lox_ir(
        struct lllil * lllil,
        struct lox_ir_control_node * control,
        struct lox_ir_guard guard
);

void emit_unary_ll_lox_ir(
        struct lllil * lllil,
        struct lox_ir_ll_operand a,
        unary_operator_type_ll_lox_ir binary_operator
);

void emit_binary_ll_lox_ir(
        struct lllil * lllil,
        binary_operator_type_ll_lox_ir binary_operator,
        struct lox_ir_ll_operand a,
        struct lox_ir_ll_operand b
);

void emit_range_check_ll_lox_ir(
        struct lllil *,
        struct lox_ir_control_set_array_element_node * control,
        struct lox_ir_ll_operand instance,
        struct lox_ir_ll_operand index
);
