#include "binary_operations.h"

#include "runtime/jit/x64/x64_jit_compiler.h"

#define NO_RETURN_VALUE -1

extern lox_value_t addition_lox(lox_value_t a, lox_value_t b);

static int imm_imm_add_operation(struct jit_compiler *, struct operand, struct operand);
static int imm_imm_sub_operation(struct jit_compiler *, struct operand, struct operand);
static int add_operation(struct jit_compiler *, struct operand, struct operand);
static void set_al_with_cmp_result(struct jit_compiler * jit_compiler, bytecode_t comparation_opcode);
static int sub_operation(struct jit_compiler *, struct operand, struct operand);
static int imm_imm_mul_operation(struct jit_compiler *, struct operand, struct operand);
static int mul_operation(struct jit_compiler *, struct operand, struct operand);
static int imm_imm_div_operation(struct jit_compiler *, struct operand, struct operand);
static int div_operation(struct jit_compiler *, struct operand, struct operand);
static int imm_imm_less_operation(struct jit_compiler *, struct operand, struct operand);
static int less_operation(struct jit_compiler *, struct operand, struct operand);
static void cast_to_lox_boolean(struct jit_compiler * jit_compiler, register_t register_casted_value);
static int imm_imm_equal_operation(struct jit_compiler *, struct operand, struct operand);
static int equal_operation(struct jit_compiler *, struct operand, struct operand);
static int imm_imm_greater_operation(struct jit_compiler *, struct operand, struct operand);
static int greater_operation(struct jit_compiler *, struct operand, struct operand);

struct binary_operation binary_operations[] = {
        [OP_GREATER] = {imm_imm_greater_operation, greater_operation, greater_operation},
        [OP_EQUAL] = {imm_imm_equal_operation, equal_operation, equal_operation},
        [OP_LESS] = {imm_imm_less_operation, less_operation, less_operation},
        [OP_ADD] = {imm_imm_add_operation, add_operation, add_operation},
        [OP_SUB] = {imm_imm_sub_operation, sub_operation, sub_operation},
        [OP_MUL] = {imm_imm_mul_operation, mul_operation, mul_operation},
        [OP_DIV] = {imm_imm_div_operation, div_operation, div_operation},
};

static int greater_operation(struct jit_compiler * jit_compiler,
                             struct operand a,
                             struct operand b) {
    emit_cmp(&jit_compiler->native_compiled_code, a, b);
    emit_setg_al(&jit_compiler->native_compiled_code);
    cast_to_lox_boolean(jit_compiler, a.as.reg);
    return NO_RETURN_VALUE;
}

static int imm_imm_greater_operation(struct jit_compiler * jit_compiler,
                                   struct operand a,
                                   struct operand b
) {
    return TO_LOX_VALUE_BOOL(a.as.immediate > b.as.immediate);
}

static int imm_imm_equal_operation(struct jit_compiler * jit_compiler,
                                  struct operand a,
                                  struct operand b
) {
    return TO_LOX_VALUE_BOOL(a.as.immediate == b.as.immediate);
}

static int equal_operation(struct jit_compiler * jit_compiler,
        struct operand a,
        struct operand b) {
    emit_cmp(&jit_compiler->native_compiled_code, a, b);
    emit_sete_al(&jit_compiler->native_compiled_code);
    cast_to_lox_boolean(jit_compiler, a.as.reg);
    return NO_RETURN_VALUE;
}

static int imm_imm_less_operation(struct jit_compiler * jit_compiler,
                                  struct operand a,
                                  struct operand b
) {
    return TO_LOX_VALUE_BOOL(a.as.immediate < b.as.immediate);
}

static int less_operation(struct jit_compiler * jit_compiler,
                          struct operand a,
                          struct operand b) {

    emit_cmp(&jit_compiler->native_compiled_code, a, b);
    emit_setl_al(&jit_compiler->native_compiled_code);
    cast_to_lox_boolean(jit_compiler, a.as.reg);
    return NO_RETURN_VALUE;
}

static int imm_imm_add_operation(
        struct jit_compiler * jit_compiler,
        struct operand a,
        struct operand b
) {
    return a.as.immediate + b.as.immediate;
}

static int add_operation(
        struct jit_compiler * jit_compiler,
        struct operand a,
        struct operand b
) {
    call_external_c_function(
            jit_compiler,
            MODE_VM_GC,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(addition_lox),
            2,
            a,
            b
    );

    emit_mov(&jit_compiler->native_compiled_code, a, RAX_REGISTER_OPERAND);

    return NO_RETURN_VALUE;
}

static int imm_imm_sub_operation(
        struct jit_compiler * jit_compiler,
        struct operand a,
        struct operand b
) {
    return a.as.immediate - b.as.immediate;
}

static int sub_operation(
        struct jit_compiler * jit_compiler,
        struct operand a,
        struct operand b
) {
    emit_sub(&jit_compiler->native_compiled_code, a, b);
    return NO_RETURN_VALUE;
}

static int imm_imm_mul_operation(
        struct jit_compiler * jit_compiler,
        struct operand a,
        struct operand b
) {
    return a.as.immediate * b.as.immediate;
}

static int mul_operation(
        struct jit_compiler * jit_compiler,
        struct operand op_reg,
        struct operand imm_reg
) {
    emit_imul(&jit_compiler->native_compiled_code, op_reg, imm_reg);
    return NO_RETURN_VALUE;
}

static int imm_imm_div_operation(
        struct jit_compiler * jit_compiler,
        struct operand a,
        struct operand b
) {
    return a.as.immediate / b.as.immediate;
}

static int div_operation(
        struct jit_compiler * jit_compiler,
        struct operand a,
        struct operand b
) {
    emit_mov(&jit_compiler->native_compiled_code, RAX_REGISTER_OPERAND, a);
    emit_idiv(&jit_compiler->native_compiled_code, b);
    emit_mov(&jit_compiler->native_compiled_code, a, RAX_REGISTER_OPERAND);
    return 1;
}

static void set_al_with_cmp_result(struct jit_compiler * jit_compiler, bytecode_t comparation_opcode) {
    switch (comparation_opcode) {
        case OP_EQUAL: emit_sete_al(&jit_compiler->native_compiled_code); break;
        case OP_GREATER: emit_setg_al(&jit_compiler->native_compiled_code); break;
        case OP_LESS: emit_setl_al(&jit_compiler->native_compiled_code); break;
        default: //TODO Panic
    }
}

static void cast_to_lox_boolean(struct jit_compiler * jit_compiler, register_t register_casted_value) {
    //If true register_casted_value will hold 1, if false it will hold 0
    emit_al_movzx(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(register_casted_value));

    //If true, register_casted_value will hold 1, if you add 2, you will get the value_node of TAG_TRUE defined in types.h
    //If false, register_casted_value will hold 0, if you add 2, you will get the value_node of TAG_FALSE defined in types.h
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_casted_value),
             IMMEDIATE_TO_OPERAND(2));

    register_t register_quiet_float_nan = push_register_allocator(&jit_compiler->register_allocator);

    //In x64 the max size of the immediate is 32 bit, FLOAT_QNAN is 64 bit.
    //We need to store FLOAT_QNAN in 64 bit register to later be able to or with register_casted_value
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_quiet_float_nan),
             IMMEDIATE_TO_OPERAND(FLOAT_QNAN));

    emit_or(&jit_compiler->native_compiled_code,
            REGISTER_TO_OPERAND(register_casted_value),
            REGISTER_TO_OPERAND(register_quiet_float_nan));

    //Dellacate register_quiet_float_nan
    pop_register_allocator(&jit_compiler->register_allocator);
}