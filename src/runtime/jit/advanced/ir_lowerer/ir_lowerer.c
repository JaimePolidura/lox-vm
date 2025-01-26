#include "ir_lowerer.h"

bool lower_lox_ir_block(struct lox_ir_block *, void *);

struct lox_level_lox_ir_node * lower_lox_ir(struct lox_ir * lox_ir) {
    struct lllil * lllil = alloc_low_level_lox_ir_lowerer(lox_ir);

    for_each_lox_ir_block(
            lox_ir->first_block,
            NATIVE_LOX_ALLOCATOR(),
            lllil,
            LOX_IR_BLOCK_OPT_NOT_REPEATED,
            lower_lox_ir_block
    );

    free_low_level_lox_ir_lowerer(lllil);

    return NULL;
}

bool lower_lox_ir_block(struct lox_ir_block * block, void * extra) {
    struct lllil * lllil = extra;

    for (struct lox_ir_control_node * current = block->first;;current = current->next) {
        lower_lox_ir_control(lllil, current);

        if (current == block->last) {
            break;
        }
    }

    return true;
}