#include "single_operations.h"

#include "runtime/jit/x64/x64_jit_compiler.h"

#define NO_RETURN_VALUE -1

static int reg_negate_operation(struct jit_compiler * jit_compiler, struct operand op);
static int imm_negate_operation(struct jit_compiler * jit_compiler, struct operand op);
static int imm_not_operation(struct jit_compiler * jit_compiler, struct operand imm);
static int reg_not_operation(struct jit_compiler * jit_compiler, struct operand op);

struct single_operation single_operations[] = {
        [OP_NEGATE] = {imm_negate_operation, reg_negate_operation},
        [OP_NOT] = {imm_not_operation, reg_not_operation},
};

static int reg_negate_operation(struct jit_compiler * jit_compiler, struct operand op) {
    emit_neg(&jit_compiler->native_compiled_code, op);
    return NO_RETURN_VALUE;
}

static int imm_negate_operation(struct jit_compiler * jit_compiler, struct operand op) {
    return -op.as.immediate;
}

//True value_node:  0x7ffc000000000003
//False value_node: 0x7ffc000000000002
//value1 = value0 + ((TRUE - value0) + (FALSE - value0))
//value1 = - value0 + TRUE + FALSE
//value1 = (TRUE - value_node) + FALSE
static int reg_not_operation(struct jit_compiler * jit_compiler, struct operand op) {
    register_t lox_boolean_value_reg = push_register_allocator(&jit_compiler->register_allocator);
    register_t value_to_negate = op.as.reg;

    emit_mov(&jit_compiler->native_compiled_code,
            REGISTER_TO_OPERAND(lox_boolean_value_reg),
            IMMEDIATE_TO_OPERAND(TRUE_VALUE));

    emit_sub(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(value_to_negate),
             REGISTER_TO_OPERAND(lox_boolean_value_reg));

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(lox_boolean_value_reg),
             IMMEDIATE_TO_OPERAND(FALSE_VALUE));

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(value_to_negate),
             REGISTER_TO_OPERAND(lox_boolean_value_reg));

    //pop lox_boolean_value_reg
    pop_register_allocator(&jit_compiler->register_allocator);

    return NO_RETURN_VALUE;
}

static int imm_not_operation(struct jit_compiler * jit_compiler, struct operand imm) {
    lox_value_t lox_boolean = imm.as.immediate;
    return TO_LOX_VALUE_BOOL(!AS_BOOL(lox_boolean));
}