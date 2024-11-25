#include "ssa_ir.h"

void remove_names_references_ssa_ir(struct ssa_ir * ssa_ir, struct u64_set removed_ssa_names) {
    FOR_EACH_U64_SET_VALUE(removed_ssa_names, removed_ssa_name_u64) {
        struct ssa_name removed_ssa_name = CREATE_SSA_NAME_FROM_U64(removed_ssa_name_u64);
        remove_name_references_ssa_ir(ssa_ir, removed_ssa_name);
    }
}

void remove_name_references_ssa_ir(struct ssa_ir * ssa_ir, struct ssa_name ssa_name_to_remove) {
    remove_u64_hash_table(&ssa_ir->ssa_definitions_by_ssa_name, ssa_name_to_remove.u16);
    remove_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, ssa_name_to_remove.u16);
}