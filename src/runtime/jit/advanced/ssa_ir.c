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

struct ssa_name allco_ssa_name_ssa_ir(struct ssa_ir * ssa_ir, int ssa_version, char * local_name) {
    int local_number = ++ssa_ir->function->n_locals;
    struct ssa_name ssa_name = CREATE_SSA_NAME(local_number, ssa_version);
    put_u8_hash_table(&ssa_ir->function->local_numbers_to_names, local_number, local_name);
    put_u8_hash_table(&ssa_ir->max_version_allocated_per_local, local_number, (void *) ssa_version);

    struct u64_set * uses = alloc_u64_set(NATIVE_LOX_ALLOCATOR());
    put_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, ssa_name.u16, uses);

    return ssa_name;
}

void add_ssa_name_use_ssa_ir(struct ssa_ir * ssa_ir, struct ssa_name ssa_name, struct ssa_control_node * ssa_control_node) {
    struct u64_set * uses = get_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, ssa_name.u16);
    add_u64_set(uses, (uint64_t) ssa_control_node);
}