#include "ir_lowerer.h"

bool lower_lox_ir_block(struct lox_ir_block *, void *);

static bool all_predecessors_have_been_scanned(struct lllil *, struct lox_ir_block *);
static void merge_predecessors_stack_slots(struct lllil *, struct lox_ir_block *);
static struct ptr_arraylist * merge_predecessors_stack_slots_lists(struct lllil*, struct ptr_arraylist*, struct ptr_arraylist*);

struct lox_level_lox_ir_node * lower_lox_ir(struct lox_ir * lox_ir) {
    struct lllil * lllil = alloc_low_level_lox_ir_lowerer(lox_ir);

    for_each_lox_ir_block(
            lox_ir->first_block,
            NATIVE_LOX_ALLOCATOR(),
            lllil,
            LOX_IR_BLOCK_OPT_REPEATED,
            lower_lox_ir_block
    );

    free_low_level_lox_ir_lowerer(lllil);

    return NULL;
}

bool lower_lox_ir_block(struct lox_ir_block * block, void * extra) {
    struct lllil * lllil = extra;

    add_u64_set(&lllil->processed_blocks, (uint64_t) block);
    if (!all_predecessors_have_been_scanned(lllil, block)) {
        return false;
    }

    merge_predecessors_stack_slots(lllil, block);

    struct lllil_control lllil_control = (struct lllil_control) {
            .control_node_to_lower = NULL,
            .last_node_lowered = NULL,
            .lllil = lllil,
    };

    for (struct lox_ir_control_node * current = block->first; current != NULL; current = current->next) {
        if (is_lowered_type_lox_ir_control(current)) {
            continue;
        }

        lllil_control.control_node_to_lower = current;

        lower_lox_ir_control(&lllil_control);
        remove_block_control_node_lox_ir(lllil->lox_ir, block, current);
    }

    return true;
}

static void merge_predecessors_stack_slots(
        struct lllil * lllil,
        struct lox_ir_block * block
) {
    struct ptr_arraylist * merged_stack_slots = NULL;

    FOR_EACH_U64_SET_VALUE(block->predecesors, current_predecessor_u64_ptr) {
        struct lox_ir_block * current_predecessor = (struct lox_ir_block *) current_predecessor_u64_ptr;
        struct ptr_arraylist * current_predecessor_stack_slots = get_u64_hash_table(&lllil->stack_slots_by_block,
                current_predecessor_u64_ptr);

        if (current_predecessor_stack_slots != NULL && merged_stack_slots == NULL) {
            merged_stack_slots = current_predecessor_stack_slots;
        } else if (current_predecessor_stack_slots != NULL && merged_stack_slots != NULL) {
            merged_stack_slots = merge_predecessors_stack_slots_lists(
                    lllil, merged_stack_slots, current_predecessor_stack_slots
            );
        }
    }
}

//a: [8, 16, 16]
//b: [8, 32, 64, 8]
//result: [8, 32, 64, 8]
static struct ptr_arraylist * merge_predecessors_stack_slots_lists(
        struct lllil * lllil,
        struct ptr_arraylist * a,
        struct ptr_arraylist * b
) {
    struct ptr_arraylist * result = alloc_ptr_arraylist(&lllil->lllil_allocator.lox_allocator);
    int current_index = 0;

    for (int i = 0; i < MAX(a->in_use, b->in_use); i++) {
        bool a_list_completed = i >= a->in_use;
        bool b_list_completed = i >= a->in_use;

        if (!a_list_completed && !b_list_completed) {
            uint64_t stack_slot_size_a = (uint64_t) a->values[i];
            uint64_t stack_slot_size_b = (uint64_t) b->values[i];
            result->values[i] = (void *) MAX(stack_slot_size_a, stack_slot_size_b);
        } else if (!a_list_completed && b_list_completed) {
            result->values[i] = a->values[i];
        } else if (a_list_completed && !b_list_completed) {
            result->values[i] = b->values[i];
        }
    }

    return result;
}

static bool all_predecessors_have_been_scanned(
        struct lllil * lllil,
        struct lox_ir_block * block
) {
    FOR_EACH_U64_SET_VALUE(block->predecesors, current_predecessor_u64_ptr) {
        if(!contains_u64_set(&lllil->processed_blocks, current_predecessor_u64_ptr)){
            return false;
        }
    }

    return true;
}