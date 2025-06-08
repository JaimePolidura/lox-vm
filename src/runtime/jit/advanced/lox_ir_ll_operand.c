#include "lox_ir_ll_operand.h"

struct u64_set get_used_v_reg_ssa_name_ll_operand(struct lox_ir_ll_operand operand, struct lox_allocator * allocator) {
    struct u64_set used_ssa_names;
    init_u64_set(&used_ssa_names, allocator);

    switch (operand.type) {
        case LOX_IR_LL_OPERAND_REGISTER: {
            add_u64_set(&used_ssa_names, operand.v_register.ssa_name.u16);
            break;
        }
        case LOX_IR_LL_OPERAND_PHI_V_REGISTER: {
            FOR_EACH_U64_SET_VALUE(operand.phi_v_register.versions, uint64_t, version) {
                struct ssa_name ssa_name_in_phi = CREATE_SSA_NAME(
                        operand.phi_v_register.v_register.ssa_name.value.local_number,
                        version
                );
                add_u64_set(&used_ssa_names, ssa_name_in_phi.u16);
            }
            break;
        }
        case LOX_IR_LL_OPERAND_MEMORY_ADDRESS: {
            add_u64_set(&used_ssa_names, operand.memory_address.address.ssa_name.u16);
            break;
        }
        case LOX_IR_LL_OPERAND_IMMEDIATE:
        case LOX_IR_LL_OPERAND_STACK_SLOT:
        case LOX_IR_LL_OPERAND_FLAGS:
            break;
        default:
            lox_assert_failed("lox_ir_ll_operand.c::get_used_v_reg_ll_operand", "Uknown operand type %i",
                              operand.type);
    }

    return used_ssa_names;
}