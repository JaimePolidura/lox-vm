#include "range_check_elimination.h"

struct rce {
    struct arena_lox_allocator rce_allocator;
    struct ssa_ir * ssa_ir;
};

static struct rce * alloc_range_check_elimination(struct ssa_ir *);
static void free_range_check_elimination(struct rce *);

void perform_range_check_elimination(struct ssa_ir * ssa_ir) {
    struct rce * range_check_elimination = alloc_range_check_elimination(ssa_ir);

    //TODO

    free_range_check_elimination(range_check_elimination);
}

static struct rce * alloc_range_check_elimination(struct ssa_ir *) {
    return NULL;
}

static void free_range_check_elimination(struct rce *) {

}