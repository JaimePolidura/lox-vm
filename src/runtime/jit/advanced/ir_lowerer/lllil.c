#include "lllil.h"

struct lllil * alloc_low_level_lox_ir_lowerer(struct lox_ir * lox_ir) {
    struct lllil * lllil = NATIVE_LOX_MALLOC(sizeof(struct lllil));
    struct arena arena;
    lllil->lox_ir = lox_ir;
    init_arena(&arena);
    lllil->lllil_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&lllil->stack_slots_by_block, &lllil->lllil_allocator.lox_allocator);
    init_u64_set(&lllil->processed_blocks, &lllil->lllil_allocator.lox_allocator);
    lllil->last_phi_resolution_v_reg_allocated = lox_ir->last_v_reg_allocated;

    return lllil;
}

void free_low_level_lox_ir_lowerer(struct lllil * lllil) {
    free_arena(&lllil->lllil_allocator.arena);
    NATIVE_LOX_FREE(lllil);
}

uint16_t allocate_stack_slot_lllil(
        struct lllil * lllil,
        struct lox_ir_block * block,
        size_t required_size
) {
    if(!contains_u64_hash_table(&lllil->stack_slots_by_block, (uint64_t) block)){
        struct ptr_arraylist * stack_slots = alloc_ptr_arraylist(&lllil->lllil_allocator.lox_allocator);
        put_u64_hash_table(&lllil->stack_slots_by_block, (uint64_t) block, stack_slots);
    }

    struct ptr_arraylist * stack_slots = get_u64_hash_table(&lllil->stack_slots_by_block, (uint64_t) block);
    append_ptr_arraylist(stack_slots, (void *) required_size);

    return stack_slots->in_use - 1;
}

void add_lowered_node_lllil_control(
        struct lllil_control * lllil_control,
        struct lox_ir_control_node * lowered_node_to_add
) {
    struct lox_ir_control_node * prev_node = lllil_control->last_node_lowered;
    struct lox_ir_block * block = lllil_control->control_node_to_lower->block;

    add_after_control_node_block_lox_ir(lllil_control->lllil->lox_ir, block, prev_node, lowered_node_to_add);

    lllil_control->last_node_lowered = lowered_node_to_add;
}