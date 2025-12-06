#include "range_check_elimination.h"

struct rce {
    struct arena_lox_allocator rce_allocator;
    struct lox_ir * lox_ir;
};

static struct rce * alloc_range_check_elimination(struct lox_ir *);
static void free_range_check_elimination(struct rce *);

void perform_range_check_elimination(struct lox_ir * lox_ir) {
    struct rce * range_check_elimination = alloc_range_check_elimination(lox_ir);

    //TODO

    free_range_check_elimination(range_check_elimination);
}

static struct rce * alloc_range_check_elimination(struct lox_ir *) {
    return NULL;
}

static void free_range_check_elimination(struct rce *) {

}