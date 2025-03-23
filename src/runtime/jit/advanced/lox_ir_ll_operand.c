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
            //TODO Runtime panic
    }
}