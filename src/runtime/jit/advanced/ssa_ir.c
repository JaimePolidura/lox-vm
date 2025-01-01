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

struct ssa_name alloc_ssa_name_ssa_ir(
        struct ssa_ir * ssa_ir,
        int ssa_version,
        char * local_name,
        struct ssa_block * block,
        struct ssa_type * type
) {
    int local_number = ++ssa_ir->function->n_locals;
    struct ssa_name ssa_name = CREATE_SSA_NAME(local_number, ssa_version);
    local_name = dynamic_format_string("%s%i", local_name, local_number);
    put_u8_hash_table(&ssa_ir->function->local_numbers_to_names, local_number, local_name);
    put_u8_hash_table(&ssa_ir->max_version_allocated_per_local, local_number, (void *) ssa_version);

    struct u64_set * uses = alloc_u64_set(NATIVE_LOX_ALLOCATOR());
    put_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, ssa_name.u16, uses);

    put_type_by_ssa_name_ssa_ir(ssa_ir, block, ssa_name, type);

    return ssa_name;
}

struct ssa_name alloc_ssa_version_ssa_ir(struct ssa_ir * ssa_ir, int local_number) {
    uint8_t last_version = (uint8_t) get_u8_hash_table(&ssa_ir->max_version_allocated_per_local, local_number);
    uint8_t new_version = last_version + 1;
    put_u8_hash_table(&ssa_ir->max_version_allocated_per_local, local_number, (void *) new_version);
    return CREATE_SSA_NAME(local_number, new_version);
}

void remove_ssa_name_use_ssa_ir(
    struct ssa_ir * ssa_ir,
    struct ssa_name ssa_name,
    struct ssa_control_node * ssa_control_node
) {
    if(contains_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, ssa_name.u16)){
        struct u64_set * uses = get_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, ssa_name.u16);
        remove_u64_set(uses, (uint64_t) ssa_control_node);
    }
}

void add_ssa_name_use_ssa_ir(
        struct ssa_ir * ssa_ir,
        struct ssa_name ssa_name,
        struct ssa_control_node * ssa_control_node
) {
    if(!contains_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, ssa_name.u16)){
        struct u64_set * uses = LOX_MALLOC(SSA_IR_ALLOCATOR(ssa_ir), sizeof(struct u64_set));
        init_u64_set(uses, SSA_IR_ALLOCATOR(ssa_ir));
        put_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, ssa_name.u16, uses);
    }

    struct u64_set * uses = get_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, ssa_name.u16);
    add_u64_set(uses, (uint64_t) ssa_control_node);
}

struct ssa_type * get_type_by_ssa_name_ssa_ir(
        struct ssa_ir * ssa_ir,
        struct ssa_block * block,
        struct ssa_name ssa_name
) {
    struct u64_hash_table * types_by_block = get_u64_hash_table(&ssa_ir->ssa_type_by_ssa_name_by_block, (uint64_t) block);
    return get_u64_hash_table(types_by_block, ssa_name.u16);
}

void put_type_by_ssa_name_ssa_ir(
        struct ssa_ir * ssa_ir,
        struct ssa_block * ssa_block,
        struct ssa_name ssa_name,
        struct ssa_type * new_type
) {
    if(new_type == NULL){
        return;
    }
    if(!contains_u64_hash_table(&ssa_ir->ssa_type_by_ssa_name_by_block, (uint64_t) ssa_block)){
        struct u64_hash_table * types_by_block = LOX_MALLOC(SSA_IR_ALLOCATOR(ssa_ir), sizeof(struct u64_hash_table));
        init_u64_hash_table(types_by_block, SSA_IR_ALLOCATOR(ssa_ir));
        put_u64_hash_table(&ssa_ir->ssa_type_by_ssa_name_by_block, (uint64_t) ssa_block, types_by_block);
    }

    struct u64_hash_table * types_by_block = get_u64_hash_table(&ssa_ir->ssa_type_by_ssa_name_by_block, (uint64_t) ssa_block);
    put_u64_hash_table(types_by_block, ssa_name.u16, new_type);

    struct u64_hash_table_iterator ssa_type_by_ssa_name_by_block_ite;
    init_u64_hash_table_iterator(&ssa_type_by_ssa_name_by_block_ite, ssa_ir->ssa_type_by_ssa_name_by_block);

    while (has_next_u64_hash_table_iterator(ssa_type_by_ssa_name_by_block_ite)) {
        struct u64_hash_table_entry current_entry = next_u64_hash_table_iterator(&ssa_type_by_ssa_name_by_block_ite);
        struct u64_hash_table * current_types_by_ssa_name = current_entry.value;

        if (contains_u64_hash_table(current_types_by_ssa_name, ssa_name.u16)) {
            put_u64_hash_table(current_types_by_ssa_name, ssa_name.u16, new_type);
        }
    }
}