#include "lllil.h"

struct lllil * alloc_low_level_lox_ir_lowerer(struct lox_ir * lox_ir) {
    struct lllil * lllil = NATIVE_LOX_MALLOC(sizeof(struct lllil));
    struct arena arena;
    lllil->lox_ir = lox_ir;
    init_arena(&arena);
    lllil->lllil_allocator = to_lox_allocator_arena(arena);
    return lllil;
}

void free_low_level_lox_ir_lowerer(struct lllil * lllil) {
    free_arena(&lllil->lllil_allocator.arena);
    NATIVE_LOX_FREE(lllil);
}