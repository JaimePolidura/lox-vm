#include "lox_ir_ll_operand.h"

struct v_register * get_used_v_reg_ll_operand(struct lox_ir_ll_operand * operand) {
    switch (operand->type) {
        case LOX_IR_LL_OPERAND_REGISTER: {
            return &operand->v_register;
        }
        case LOX_IR_LL_OPERAND_MEMORY_ADDRESS: {
            return &operand->memory_address.address;
        }
        case LOX_IR_LL_OPERAND_IMMEDIATE:
        case LOX_IR_LL_OPERAND_STACK_SLOT:
        case LOX_IR_LL_OPERAND_FLAGS:
            return NULL;
        default:
            lox_assert_failed("lox_ir_ll_operand.c::get_used_v_reg_ll_operand", "Uknown operand type %i",
                              operand->type);
    }
}