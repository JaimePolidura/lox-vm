#pragma once

#include "runtime/jit/advanced/phi_resolution/v_register.h"

#include "shared/utils/collections/u64_set.h"
#include "shared/bytecode/bytecode.h"
#include "shared/utils/assert.h"
#include "shared.h"

#define ADDRESS_TO_OPERAND(address_f, offset_f) ((struct lox_ir_ll_operand) {.type = LOX_IR_LL_OPERAND_MEMORY_ADDRESS, .memory_address = {.address = (address_f), .offset = (offset_f)}})

#define IMMEDIATE_TO_OPERAND(immediate) ((struct lox_ir_ll_operand) {.type = LOX_IR_LL_OPERAND_IMMEDIATE, .immedate = (immediate)})
#define V_REG_TO_OPERAND(v_reg) ((struct lox_ir_ll_operand) {.type = LOX_IR_LL_OPERAND_REGISTER, .v_register = (v_reg)})
#define STACKSLOT_TO_OPERAND(slot_index_f, offset_f) ((struct lox_ir_ll_operand) {.type = LOX_IR_LL_OPERAND_STACK_SLOT, .stack_slot = {.slot_index = (slot_index_f), .offset = (offset_f)}})
#define FLAGS_OPERAND(flags_operator) ((struct lox_ir_ll_operand) {LOX_IR_LL_OPERAND_FLAGS, (flags_operator)})
#define PHI_V_REGISTER_OPERAND(v_register_arg, versions_arg) ((struct lox_ir_ll_operand) {.type = LOX_IR_LL_OPERAND_PHI_V_REGISTER, .phi_v_register = {.v_register = (v_register_arg), .versions = (versions_arg)}})

typedef enum {
    LOX_IR_LL_OPERAND_REGISTER,
    LOX_IR_LL_OPERAND_PHI_V_REGISTER,
    LOX_IR_LL_OPERAND_IMMEDIATE,
    LOX_IR_LL_OPERAND_MEMORY_ADDRESS,
    LOX_IR_LL_OPERAND_STACK_SLOT,
    LOX_IR_LL_OPERAND_FLAGS
} low_ir_ll_operand_type_type;

struct lox_ir_ll_operand {
    low_ir_ll_operand_type_type type;

    union {
        struct v_register v_register;
        struct {
            struct v_register v_register;
            struct u64_set versions;
        } phi_v_register;
        int64_t immedate;
        bytecode_t flags_operator;
        struct {
            uint64_t slot_index;
            int offset;
        } stack_slot;
        struct {
            struct v_register address;
            int offset;
        } memory_address;
    };
};

struct u64_set get_used_v_reg_ssa_name_ll_operand(struct lox_ir_ll_operand, struct lox_allocator *);
